#include "client.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>

#define THREAD_NUMBER 8
#define MAX_REQUESTS 200000
#define MAX_FD 300000
#define FLAG false

std::vector<const char*> local_ips{"127.0.0.1", "127.0.0.2", "127.0.0.3",
                                   "127.0.0.4", "127.0.0.5", "127.0.0.6",
                                   "127.0.0.7"};

extern void close_socket(
    int sockfd, std::unordered_map<int, std::shared_ptr<Conn>>& clients,
    locker& m_locker);
extern void* print_info(void* para);
// 从线程
void* client_thd_func(void* para) {
  // 取参数
  client_thread_args* arg = (client_thread_args*)para;
  locker& m_locker = *(arg->lo);
  int epoll_fd = arg->ep;
  int connection_count = arg->cnt;
  // unordered_map<connfd, Conn*>
  std::unordered_map<int, std::shared_ptr<Conn>>& clients = *(arg->co);
  Threadpool<Conn>* pool = arg->po;
  struct epoll_event events[connection_count];

  while (true) {
    // 等待epoll事件
    int epoll_count = epoll_wait(epoll_fd, events, connection_count, -1);
    if (epoll_count < 0 && errno != EINTR) {
      std::cout << "epoll failure." << std::endl;
      break;
    }
    for (int i = 0; i < epoll_count; ++i) {
      int sockfd = events[i].data.fd;
      if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        close_socket(sockfd, clients, m_locker);
      } else if (events[i].events & EPOLLIN) {
        m_locker.lock();
        if (!clients[sockfd]->get_message()) {
          close_socket(sockfd, clients, m_locker);
        }
        m_locker.lock();
      }
    }
  }
  return nullptr;
}

// 建立连接
bool create_connect(const char* ip, struct sockaddr_in& server_addr, int opt,
                    std::unordered_map<int, std::shared_ptr<Conn>>& clients,
                    Threadpool<Conn>* pool, int epoll_fd, locker& m_locker) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = 0;
  inet_pton(AF_INET, ip, &local_addr.sin_addr);
  if (bind(socket_fd, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
    return false;
  }
  if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "connect failure" << std::endl;
    return false;
  }
  clients[socket_fd] = std::make_shared<Conn>(FLAG);
  clients[socket_fd]->init(socket_fd, epoll_fd);
  return true;
}

// 解析命令行传进来的地址
void parse_address(const std::string& input, std::string& ip, int& port) {
  size_t colon_pos = input.find(':');
  if (colon_pos == std::string::npos) {
    std::cerr << "Invalid input format." << std::endl;
    return;
  }

  ip = input.substr(0, colon_pos);
  port = std::stoi(input.substr(colon_pos + 1));
}

// 客户端主线程
void run_client(int connection_count, const std::string& input) {
  locker m_thread_locker;  // 主从线程之间
  locker m_conn_locker;    // clients map各个元素之间
  struct sockaddr_in server_addr;
  std::string server_ip;
  int server_port;
  Conn::m_locker_ = m_conn_locker;

  Threadpool<Conn>* pool = new Threadpool<Conn>(THREAD_NUMBER, MAX_REQUESTS);
  std::unordered_map<int, std::shared_ptr<Conn>> clients;

  // 解析地址
  parse_address(input, server_ip, server_port);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

  int epoll_fd = epoll_create1(0);
  struct epoll_event event, events[connection_count];

  int opt = 1;
  int loop_count = connection_count;
  pthread_t tid;
  bool is_server = false;
  pthread_create(&tid, nullptr, print_info, (void*)&is_server);
  pthread_detach(tid);

  // 循环创建连接
  while (loop_count > 0) {
    for (const char*& s : local_ips) {
      if (!create_connect(s, server_addr, opt, clients, pool, epoll_fd,
                          m_conn_locker)) {
        exit(1);
      }
      --loop_count;
      if (loop_count <= 0) {
        break;
      }
    }
  }
  std::cout << "done" << std::endl;
  while (true) {
    sleep(1);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
      pool->append(it->second);
    }
    // 等待epoll事件
    int epoll_count = epoll_wait(epoll_fd, events, connection_count, -1);
    if (epoll_count < 0 && errno != EINTR) {
      std::cout << "epoll failure." << std::endl;
      break;
    }
    for (int i = 0; i < epoll_count; ++i) {
      int sockfd = events[i].data.fd;
      if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        close_socket(sockfd, clients, m_thread_locker);
      } else if (events[i].events & EPOLLIN) {
        if (!clients[sockfd]->get_message()) {
          close_socket(sockfd, clients, m_thread_locker);
        }
      }
    }
  }

  close(epoll_fd);
  delete pool;
  clients.clear();
  return;
}
