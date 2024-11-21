#ifndef TCP_TEST_INCLUDE_CLIENT_H_
#define TCP_TEST_INCLUDE_CLIENT_H_

#include "conn.h"
#include "threadpool.h"
#include "server.h"
#include <string>
#include <memory>
#include <unordered_map>

void run_client(int connection_count,
                const std::string& address);  // 客户端主线程
void* client_thd_func(void* para);            // epoll_wait线程
void parse_address(const std::string& input, std::string& ip,
                   int& port);  // 解析地址
bool create_connect(const char* ip, struct sockaddr_in& server_addr, int opt,
                    std::unordered_map<int, std::shared_ptr<Conn>>& clients,
                    Threadpool<Conn>* pool, int epoll_fd, locker& m_locker);

// 从线程传入参数
struct client_thread_args {
  locker* lo;  // 互斥锁
  int ep;      // 线程对应的epoll实例
  int cnt;     // 指令传入的连接数
  std::unordered_map<int, std::shared_ptr<Conn>>* co;  // 连接池
  struct Threadpool<Conn>* po;                         // 线程池
};

#endif  // TCP_TEST_INCLUDE_CLIENT_H_
