/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_PUBLIC_LINUX_SYSCALLS_INCLUDE_SYS_SOCKET_H_
#define NATIVE_CLIENT_SRC_PUBLIC_LINUX_SYSCALLS_INCLUDE_SYS_SOCKET_H_

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_SEQPACKET 5

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

typedef uint32_t socklen_t;

struct iovec {
  void *iov_base;
  size_t iov_len;
};

struct msghdr {
  void *msg_name;
  socklen_t msg_namelen;

  struct iovec *msg_iov;
  size_t msg_iovlen;

  void *msg_control;
  size_t msg_controllen;

  int msg_flags;
};

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
int shutdown(int sockfd, int how);
int socketpair(int domain, int type, int protocol, int sv[2]);

#ifdef __cplusplus
}
#endif

#endif
