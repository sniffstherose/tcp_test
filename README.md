# tcp_test

`tcp_test`是一个简单的防火墙测试工具，通过建立多连接的TCP通信来实现。

## 项目结构

```
include/	头文件
src/		源文件
```

## 功能

- **服务端模式**：启动TCP服务，监听指定端口并接收客户端连接；
- **客户端模式**：启动多个TCP客户端连接到指定服务器，进行测试通信。

## 编译

工具：cmake、g++

1. 安装编译工具：
   ubuntu：`sudo apt install cmake g++`
2. 编译：
   ① 进入项目根目录，执行 `mkdir build`；
   ② `cd build`进入build目录；
   ③ `cmake ..`生成Makefile文件后执行 `make`生成可执行文件 `tcp_test`。

## 运行

1. 启动服务器：
   在指定端口启动服务器：`./tcp_test server <port>`
   例如，在端口9527启动服务器：`./tcp_test server 9527`
2. 启动客户端
   启动多个客户端连接到指定服务器地址和端口：`./tcp_test echo -c <connection_count> -s <server_ip>:<server_port>`
   例如，启动20000个TCP连接到localhost的9527端口：

   ```bash
   ./tcp_test -c 20000 -s localhost:9527
   ```
3. 参数说明：

   - server：启动服务器模式。

     - `<port>：服务端监听的端口号。`
   - echo：启动客户端模式。

     - -c：指定要创建的连接数。
     - -s：指定服务器地址和端口。

     -c和-s参数与顺序无关

## 注意事项

确保在高并发情况下，有足够的系统资源（文件描述符、内存、可用端口号等）来处理大量连接。

## 联系方式

- 邮箱：songtao-ni@outlook.com
