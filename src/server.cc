#include "server.h"
#include <sys/epoll.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <string>
#include <vector>

#define THREAD_NUMBER 8  // 线程池线程数
#define SUB_THREAD_NUMBER 5
#define MAX_REQUESTS 200000      // 任务队列最大请求数
#define MAX_FD 300000            // 最大文件描述符
#define MAX_EVENT_NUMBER 200000  // 监听的最大事件数量
#define FLAG true                // 标记是否为服务端

// 关闭套接字的函数
void close_socket(int sockfd,
                  std::unordered_map<int, std::shared_ptr<Conn>>& clients,
                  locker& m_locker) {
  m_locker.lock();
  if (clients.find(sockfd) != clients.end()) {
    clients[sockfd]->close_conn();
    clients.erase(sockfd);
  }
  m_locker.unlock();
}

// 从线程函数
void* server_thd_func(void* para) {
  server_thread_args* arg = static_cast<server_thread_args*>(para);
  locker& m_locker = *(arg->lo);
  std::unordered_map<int, std::shared_ptr<Conn>>& clients = *(arg->cl);
  Threadpool<Conn>* pool = arg->po;
  int epoll_fd = arg->ep;
  int events_num = MAX_EVENT_NUMBER / 5;
  struct epoll_event events[events_num];

  while (true) {
    int epoll_count = epoll_wait(epoll_fd, events, events_num, -1);
    if (epoll_count < 0 && errno != EINTR) {
      std::cout << "epoll failure." << std::endl;
      break;
    }

    for (int i = 0; i < epoll_count; ++i) {  // 处理epoll事件
      int sockfd = events[i].data.fd;
      if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        close_socket(sockfd, clients, m_locker);
      } else if (events[i].events & EPOLLIN) {  // 有数据可读
        m_locker.lock();
        pool->append(clients[sockfd]);
        m_locker.unlock();
      } else if (events[i].events & EPOLLOUT) {  // 有数据可写
        m_locker.lock();
        bool isSend = clients[sockfd]->send_message();
        m_locker.unlock();
        if (!isSend) {
          close_socket(sockfd, clients, m_locker);
        }
      }
    }
  }
  delete arg;
  return nullptr;
}

void* print_info(void* para) {
  bool flag = *(bool*)para;
  long long lastPing = 0;
  long long lastPong = 0;
  while (true) {
    sleep(1);
    if (flag) {
      std::cout << "Received " << Conn::ping_count_ - lastPing << " ping. "
                << "Sent " << Conn::pong_count_ - lastPong << " pong." << std::endl;
    } else {
      std::cout << "Sent " << Conn::ping_count_ - lastPing << " ping. "
                << "Received " << Conn::pong_count_ - lastPong << " pong." << std::endl;
    }
    lastPing = Conn::ping_count_;
    lastPong = Conn::pong_count_;
  }
}

// 服务端主线程
void run_server(int port) {
  locker m_thread_locker;  // 主从线程之间
  locker m_conn_locker;    // clients map各个元素之间
  int server_fd;
  struct sockaddr_in server_addr;
  int opt = 1;
  Threadpool<Conn>* pool = new Threadpool<Conn>(THREAD_NUMBER, MAX_REQUESTS);
  std::unordered_map<int, std::shared_ptr<Conn>> clients;
  pthread_t threads[SUB_THREAD_NUMBER];  // 从线程数组
  int epoll_fds[SUB_THREAD_NUMBER];      // 从线程对应epoll实例数组
  Conn::m_locker_ = m_conn_locker;

  // 初始化epoll实例
  for (int i = 0; i < SUB_THREAD_NUMBER; ++i) {
    epoll_fds[i] = epoll_create1(0);
  }
  // 创建线程
  for (int i = 0; i < SUB_THREAD_NUMBER; ++i) {
    server_thread_args* a = new server_thread_args();
    a->lo = &m_thread_locker;
    a->ep = epoll_fds[i];
    a->cl = &clients;
    a->po = pool;
    pthread_create(threads + i, nullptr, server_thd_func,
                   static_cast<void*>(a));
    pthread_detach(*(threads + i));
  }

  pthread_t tid;
  bool is_server = true;
  pthread_create(&tid, nullptr, print_info, (void*)&is_server);
  pthread_detach(tid);

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt,
             sizeof(opt));  // 端口复用
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  listen(server_fd, 100);

  // select等待客户端连接
  fd_set listen_set;
  FD_ZERO(&listen_set);
  int ret = 0;
  while (true) {
    FD_SET(server_fd, &listen_set);
    ret = select(server_fd + 1, &listen_set, nullptr, nullptr, nullptr);
    if (ret < 0) {
      std::cerr << "select failure" << std::endl;
    }
    if (FD_ISSET(server_fd, &listen_set)) {
      struct sockaddr_in client_addr;
      socklen_t addr_len = sizeof(client_addr);
      int connfd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
      if (connfd >= MAX_FD) {
        close(connfd);
        continue;
      }
      m_thread_locker.lock();
      clients[connfd] = std::make_shared<Conn>(FLAG);
      clients[connfd]->init(connfd, epoll_fds[connfd % SUB_THREAD_NUMBER]);
      m_thread_locker.unlock();
    }
    if (Conn::clients_count_ % 1000 == 0) {
      std::cout << Conn::clients_count_ << std::endl;
    }
  }

  for (int i = 0; i < SUB_THREAD_NUMBER; ++i) {
    close(epoll_fds[i]);
  }
  close(server_fd);
  delete pool;
  clients.clear();
  return;
}