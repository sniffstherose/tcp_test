#include "conn.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <iostream>

// 一些辅助函数
inline int setnonblocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

inline void addfd(int epollfd, int fd, bool one_shot, Conn* conn) {
  epoll_event event;
  event.data.fd = fd;
  // 服务端关心in事件，客户端关心out事件
  event.events = EPOLLRDHUP | (conn->flag_ ? EPOLLIN : EPOLLOUT);
  if (one_shot) {
    event.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

inline void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

inline void modfd(int epollfd, int fd, int ev) {
  epoll_event event;
  event.data.fd = fd;
  event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int Conn::clients_count_ = 0;
long long Conn::ping_count_ = 0;
long long Conn::pong_count_ = 0;
locker Conn::m_locker_{};

Conn::Conn(bool flag) : flag_(flag) {
  if (flag) {  // 服务端
    get_ = PING;
    send_ = PONG;
  } else {  // 客户端
    get_ = PONG;
    send_ = PING;
  }
};

void Conn::close_conn() {
  if (sockfd_ != -1) {
    removefd(epoll_fd_, sockfd_);
    sockfd_ = -1;
    m_locker_.lock();
    --clients_count_;
    m_locker_.unlock();
  }
}

void Conn::init(int sockfd, int epoll_fd) {
  sockfd_ = sockfd;
  epoll_fd_ = epoll_fd;

  int opt = 1;
  setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
             sizeof(opt));
  addfd(epoll_fd_, sockfd_, true, this);
  m_locker_.lock();
  ++clients_count_;
  m_locker_.unlock();
}

bool Conn::get_message() {
  int bytes_read = 0;
  bytes_read = read(sockfd_, read_buf_, sizeof(read_buf_));
  if (bytes_read == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return true;
    }
    return false;
  } else if (bytes_read == 0) {
    return false;
  } else if (strcmp(read_buf_, get_) != 0) {
    return false;
  }
  m_locker_.lock();
  flag_ ? ++ping_count_ : ++pong_count_;
  m_locker_.unlock();
  return true;
}

void Conn::process() {
  if (flag_) {  // 服务端默认先收
    if (get_message()) {
      modfd(epoll_fd_, sockfd_, EPOLLOUT);
    }
  } else {  // 客户端默认先发
    if (send_message()) {
      modfd(epoll_fd_, sockfd_, EPOLLIN);
    }
  }
}

bool Conn::send_message() {
  int bytes_write;
  bytes_write = write(sockfd_, send_, 5);
  if (bytes_write == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return true;
    }
    return false;
  } else if (bytes_write == 0) {
    return false;
  }
  if (flag_) {  // 服务端
    modfd(epoll_fd_, sockfd_, EPOLLIN);
  }
  m_locker_.lock();
  flag_ ? ++pong_count_ : ++ping_count_;
  m_locker_.unlock();
  return true;
}
