#ifndef TCP_TEST_INCLUDE_THREADPOOL_H_
#define TCP_TEST_INCLUDE_THREADPOOL_H_

#include "locker.h"
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <exception>
#include <memory>

template <typename T>
class Threadpool {
 public:
  Threadpool(int thread_number, int max_requests);
  ~Threadpool();
  bool append(std::shared_ptr<T> request);

 private:
  static void* worker(void* arg);  // 线程回调函数
  void run();                      // 处理函数

 private:
  int m_thread_number_;   // 线程数
  pthread_t* m_threads_;  // 线程数组
  int m_max_requests_;    // 任务队列最大长度
  std::vector<std::shared_ptr<T>> m_workqueue_ {};  // 任务队列（循环利用空间）
  int m_insert_index_;             // 插入的任务总数
  int m_current_index_;            // 处理的任务总数
  sem m_queuestat_;                // 信号量
  bool m_stop_;                    // 标记线程池是否运行
  locker m_queuelocker_;           // 互斥锁
};

template <typename T>
Threadpool<T>::Threadpool(int thread_number, int max_requests)
    : m_thread_number_(thread_number),
      m_max_requests_(max_requests),
      m_stop_(false),
      m_threads_(NULL) {
  if ((thread_number <= 0) || (max_requests <= 0)) {
    throw std::exception();
  }

  m_workqueue_.resize(max_requests);
  m_insert_index_ = 0;
  m_current_index_ = 0;
  m_threads_ = new pthread_t[m_thread_number_];
  if (!m_threads_) {
    throw std::exception();
  }

  for (int i = 0; i < thread_number; ++i) {
    printf("create the %dth thread\n", i);

    if (pthread_create(m_threads_ + i, NULL, worker, this)) {
      delete[] m_threads_;
      throw std::exception();
    }

    if (pthread_detach(m_threads_[i])) {
      delete[] m_threads_;
      throw std::exception();
    }
  }
}

template <typename T>
Threadpool<T>::~Threadpool() {
  delete[] m_threads_;
  m_stop_ = true;
}

// 往队列添加任务
template <typename T>
bool Threadpool<T>::append(std::shared_ptr<T> request) {
  m_queuelocker_.lock();
  // 队列满了
  if (m_insert_index_ - m_current_index_ >= m_max_requests_) {
    m_queuelocker_.unlock();
    return false;
  }
  m_workqueue_[m_insert_index_ % m_max_requests_] = request;
  ++m_insert_index_;
  m_queuelocker_.unlock();
  m_queuestat_.post();
  return true;
}

template <typename T>
void* Threadpool<T>::worker(void* arg) {
  Threadpool* pool = (Threadpool*)arg;
  pool->run();
  return pool;
}

template <typename T>
void Threadpool<T>::run() {
  while (!m_stop_) {
    m_queuestat_.wait();
    m_queuelocker_.lock();
    if (m_insert_index_ - m_current_index_ == 0) {
      m_queuelocker_.unlock();
      continue;
    }
    std::shared_ptr<T> request = m_workqueue_[m_current_index_ % m_max_requests_];
    ++m_current_index_;
    m_queuelocker_.unlock();
    if (!request) {
      continue;
    }
    request->process();  // conn.cc
  }
}

#endif  // TCP_TEST_INCLUDE_THREADPOOL_H_
