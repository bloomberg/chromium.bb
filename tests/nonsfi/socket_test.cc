/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/public/linux_syscalls/poll.h"
#include "native_client/src/public/linux_syscalls/sys/socket.h"

namespace {

char kMessage[] = "foo";
size_t kMessageLen = sizeof(kMessage);

class ScopedSocketPair {
 public:
  explicit ScopedSocketPair(int type) {
    int rc = socketpair(AF_UNIX, type, 0, fds_);
    ASSERT_EQ(rc, 0);
  }

  ~ScopedSocketPair() {
    int rc = close(fds_[0]);
    ASSERT_EQ(rc, 0);
    rc = close(fds_[1]);
    ASSERT_EQ(rc, 0);
  }

  int operator[](int i) const {
    ASSERT(i == 0 || i == 1);
    return fds_[i];
  }

 private:
  int fds_[2];

  DISALLOW_COPY_AND_ASSIGN(ScopedSocketPair);
};

void SendPacket(const ScopedSocketPair &fds) {
  struct msghdr sent_msg;
  memset(&sent_msg, 0, sizeof(sent_msg));
  struct iovec sent_iov = { kMessage, kMessageLen };
  sent_msg.msg_iov = &sent_iov;
  sent_msg.msg_iovlen = 1;
  int rc = sendmsg(fds[1], &sent_msg, 0);
  ASSERT_EQ(rc, kMessageLen);
}

char *RecvPacket(const ScopedSocketPair &fds) {
  struct msghdr received_msg;
  memset(&received_msg, 0, sizeof(received_msg));
  nacl::scoped_array<char> buf(new char[kMessageLen]);
  struct iovec received_iov = { buf.get(), kMessageLen };
  received_msg.msg_iov = &received_iov;
  received_msg.msg_iovlen = 1;

  int rc = recvmsg(fds[0], &received_msg, 0);
  ASSERT_EQ(rc, kMessageLen);
  return buf.release();
}

void TestDgramSocketpair(const char *type_str, int type) {
  printf("TestDgramSocketpair (%s)", type_str);
  ScopedSocketPair fds(type);

  SendPacket(fds);
  nacl::scoped_array<char> received(RecvPacket(fds));
  ASSERT_EQ(0, strcmp(received.get(), kMessage));
}

void TestStreamSocketpair() {
  printf("TestStreamSocketpair");
  ScopedSocketPair fds(SOCK_STREAM);

  size_t written_len = write(fds[1], kMessage, kMessageLen);
  ASSERT_EQ(written_len, kMessageLen);

  nacl::scoped_array<char> buf(new char[kMessageLen]);
  size_t read_len = read(fds[0], buf.get(), kMessageLen);
  ASSERT_EQ(read_len, kMessageLen);

  ASSERT_EQ(0, strcmp(buf.get(), kMessage));
}

void PreparePollFd(const ScopedSocketPair &fds, struct pollfd pfd[2]) {
  memset(pfd, 0, sizeof(*pfd) * 2);
  pfd[0].fd = fds[0];
  pfd[0].events = POLLIN;
  pfd[1].fd = fds[1];
  pfd[1].events = POLLOUT;
}

void TestPoll() {
  printf("TestPoll");
  ScopedSocketPair fds(SOCK_DGRAM);

  struct pollfd pfd[2];
  PreparePollFd(fds, pfd);

  int rc = poll(pfd, 2, 0);
  ASSERT_EQ(rc, 1);
  ASSERT_EQ(pfd[0].revents, 0);
  ASSERT_EQ(pfd[1].revents, POLLOUT);

  SendPacket(fds);

  PreparePollFd(fds, pfd);

  rc = poll(pfd, 2, 0);
  ASSERT_EQ(rc, 2);
  ASSERT_EQ(pfd[0].revents, POLLIN);
  ASSERT_EQ(pfd[1].revents, POLLOUT);

  nacl::scoped_array<char> buf(RecvPacket(fds));

  rc = shutdown(fds[1], SHUT_RDWR);
  ASSERT_EQ(rc, 0);

  PreparePollFd(fds, pfd);
  rc = poll(pfd, 2, 0);
  ASSERT_EQ(rc, 1);
  ASSERT_EQ(pfd[0].revents, 0);
  ASSERT_EQ(pfd[1].revents, POLLOUT | POLLHUP);
}

}  // namespace

int main(int argc, char *argv[]) {
  TestDgramSocketpair("DGRAM", SOCK_DGRAM);
  TestDgramSocketpair("SEQPACKET", SOCK_SEQPACKET);
  TestStreamSocketpair();
  TestPoll();
  return 0;
}
