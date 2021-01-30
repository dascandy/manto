#pragma once

#include "manto/async_syscall.h"
#include "manto/future.h"
#include "manto/network_address.h"
#include <span>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

struct tcp_socket {
  friend struct tcp_listen_socket;
  tcp_socket()
  : fd(-1)
  {}
  tcp_socket(network_address target, int fd) 
  : target(target)
  , fd(fd)
  {
  }
  static future<tcp_socket> create(network_address target)
  {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    co_await async_connect(fd, target.sockaddr(), target.length());
    co_return tcp_socket(target, fd);
  }
  tcp_socket(tcp_socket&& rhs) {
    fd = rhs.fd;
    target = rhs.target;
    rhs.target = {};
    rhs.fd = -1;
  }
  tcp_socket& operator=(tcp_socket&& rhs) {
    fd = rhs.fd;
    target = rhs.target;
    rhs.target = {};
    rhs.fd = -1;
    return *this;
  }
  ~tcp_socket() {
    close(fd);
  }
  future<size_t> recvmsg(uint8_t* p, size_t count) {
    struct iovec iov = { p, count };
    struct msghdr hdr;
    hdr.msg_name = nullptr;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    ssize_t recvres = co_await async_recvmsg(fd, &hdr, 0);
    if (recvres == -1) {
      exit(-1);
    } else {
      co_return recvres;
    }
  }
  future<Void> sendmsg(std::span<const uint8_t> msg) {
    struct iovec iov = { (void*)msg.data(), msg.size() };
    struct msghdr hdr;
    hdr.msg_name = nullptr;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
    ssize_t res = co_await async_sendmsg(fd, &hdr, 0);
    if (res == -1) {
      printf("async_sendmsg error");
      exit(-1);
    }
    co_return {};
  }
private:
  network_address target;
  int fd;
};

struct tcp_listen_socket {
  tcp_listen_socket(network_address listen_address, std::function<void(tcp_socket)> onConnect) {
    // TODO: handle errors
    fd = socket(AF_INET, SOCK_STREAM, 0);
    bind(fd, listen_address.sockaddr(), listen_address.length());
    listen(fd, 5);
    acceptLoopF = acceptLoop(std::move(onConnect));
  }
  future<Void> acceptLoop(std::function<void(tcp_socket)> onConnect) {
    while (not done) {
      network_address addr;
      addr.resize(sizeof(sockaddr_in6));
      int newFd = co_await async_accept(fd, addr.sockaddr(), &addr.length(), 0);
//      if (error)  TODO
      onConnect(tcp_socket(addr, newFd));
    }
    co_return {};
  }
  ~tcp_listen_socket() {
    done = true;
    // how to interrupt loop?
    close(fd);
  }
  std::atomic<bool> done{false};
  int fd;
  future<Void> acceptLoopF;
};

