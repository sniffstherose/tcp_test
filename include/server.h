#ifndef TCP_TEST_INCLUDE_SERVER_H_
#define TCP_TEST_INCLUDE_SERVER_H_

#include "conn.h"
#include "locker.h"
#include "threadpool.h"
#include <unordered_map>
#include <memory>

void run_server(int port);
void close_socket(int sockfd,
                  std::unordered_map<int, std::shared_ptr<Conn>>& clients,
                  locker& m_locker);
void* server_thd_func(void* para);

// 从线程传入参数
struct server_thread_args {
  locker* lo;  // 互斥锁
  int ep;      // 线程对应的epoll实例
  std::unordered_map<int, std::shared_ptr<Conn>>* cl;  // 连接池
  Threadpool<Conn>* po;                                // 线程池
};

#endif  // TCP_TEST_INCLUDE_SERVER_H_