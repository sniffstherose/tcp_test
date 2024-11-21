#ifndef TCP_TEST_INCLUDE_CONN_H_
#define TCP_TEST_INCLUDE_CONN_H_

#include "locker.h"
#include <arpa/inet.h>

#define PING "ping\0"
#define PONG "pong\0"

class Conn {
 public:
  static const int READ_BUFFER_SIZE = 1024;  // 读缓冲区大小
 public:
  Conn(bool flag);
  ~Conn() {};

 public:
  void close_conn();  // 关闭连接
  void init(int fd, int epoll_fd);  // 初始化函数
  void process();
  bool get_message();   // 接收消息
  bool send_message();  // 发送消息
 public:
  int epoll_fd_;
  bool flag_;                 // true表示服务器，false表示客户端
 public:
  static int clients_count_;  // 总的连接数，测试用
  static long long ping_count_;
  static long long pong_count_;
  static locker m_locker_;
 private:
  int sockfd_;  // 连接对应的socket
  sockaddr_in client_addr_;
  char* get_;   // 需要接收的消息
  char* send_;  // 需要发送的消息

  char read_buf_[READ_BUFFER_SIZE] = {0};  // 读缓冲区
};

#endif  // TCP_TEST_INCLUDE_CONN_H_