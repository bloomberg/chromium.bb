// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/safe_strerror_posix.h"
#include "tools/android/common/net.h"

// This is used in Close and Shutdown.
// Preserving errno for Close() is important because the function is very often
// used in cleanup code, after an error occurred, and it is very easy to pass an
// invalid file descriptor to close() in this context, or more rarely, a
// spurious signal might make close() return -1 + setting errno to EINTR,
// masking the real reason for the original error. This leads to very unpleasant
// debugging sessions.
#define PRESERVE_ERRNO_HANDLE_EINTR(Func)                     \
  do {                                                        \
    int local_errno = errno;                                  \
    (void) HANDLE_EINTR(Func);                                \
    errno = local_errno;                                      \
  } while (false);


namespace forwarder2 {

bool Socket::BindUnix(const std::string& path, bool abstract) {
  errno = 0;
  if (!InitUnixSocket(path, abstract) || !BindAndListen()) {
    Close();
    return false;
  }
  return true;
}

bool Socket::BindTcp(const std::string& host, int port) {
  errno = 0;
  if (!InitTcpSocket(host, port) || !BindAndListen()) {
    Close();
    return false;
  }
  return true;
}

bool Socket::ConnectUnix(const std::string& path, bool abstract) {
  errno = 0;
  if (!InitUnixSocket(path, abstract) || !Connect()) {
    Close();
    return false;
  }
  return true;
}

bool Socket::ConnectTcp(const std::string& host, int port) {
  errno = 0;
  if (!InitTcpSocket(host, port) || !Connect()) {
    Close();
    return false;
  }
  return true;
}

Socket::Socket()
    : socket_(-1),
      port_(0),
      socket_error_(false),
      family_(AF_INET),
      abstract_(false),
      addr_ptr_(reinterpret_cast<sockaddr*>(&addr_.addr4)),
      addr_len_(sizeof(sockaddr)),
      exit_notifier_fd_(-1) {
  memset(&addr_, 0, sizeof(addr_));
}

Socket::~Socket() {
  CHECK(IsClosed());
}

void Socket::Shutdown() {
  if (!IsClosed()) {
    PRESERVE_ERRNO_HANDLE_EINTR(shutdown(socket_, SHUT_RDWR));
  }
}

void Socket::Close() {
  if (!IsClosed()) {
    PRESERVE_ERRNO_HANDLE_EINTR(close(socket_));
    socket_ = -1;
  }
}

bool Socket::InitSocketInternal() {
  socket_ = socket(family_, SOCK_STREAM, 0);
  if (socket_ < 0)
    return false;
  tools::DisableNagle(socket_);
  int reuse_addr = 1;
  setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
             &reuse_addr, sizeof(reuse_addr));
  tools::DeferAccept(socket_);
  return true;
}

bool Socket::InitUnixSocket(const std::string& path, bool abstract) {
  static const size_t kPathMax = sizeof(addr_.addr_un.sun_path);
  // For abstract sockets we need one extra byte for the leading zero.
  if ((abstract && path.size() + 2 /* '\0' */ > kPathMax) ||
      (!abstract && path.size() + 1 /* '\0' */ > kPathMax)) {
    LOG(ERROR) << "The provided path is too big to create a unix "
               << "domain socket: " << path;
    return false;
  }
  abstract_ = abstract;
  family_ = PF_UNIX;
  addr_.addr_un.sun_family = family_;

  if (abstract) {
    // Copied from net/base/unix_domain_socket_posix.cc
    // Convert the path given into abstract socket name. It must start with
    // the '\0' character, so we are adding it. |addr_len| must specify the
    // length of the structure exactly, as potentially the socket name may
    // have '\0' characters embedded (although we don't support this).
    // Note that addr_.addr_un.sun_path is already zero initialized.
    memcpy(addr_.addr_un.sun_path + 1, path.c_str(), path.size());
    addr_len_ = path.size() + offsetof(struct sockaddr_un, sun_path) + 1;
  } else {
    memcpy(addr_.addr_un.sun_path, path.c_str(), path.size());
    addr_len_ = sizeof(sockaddr_un);
  }

  addr_ptr_ = reinterpret_cast<sockaddr*>(&addr_.addr_un);
  return InitSocketInternal();
}

bool Socket::InitTcpSocket(const std::string& host, int port) {
  port_ = port;

  if (host.empty()) {
    // Use localhost: INADDR_LOOPBACK
    family_ = AF_INET;
    addr_.addr4.sin_family = family_;
    addr_.addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else if (!Resolve(host)) {
    return false;
  }
  CHECK(family_ == AF_INET || family_ == AF_INET6)
      << "Invalid socket family.";
  if (family_ == AF_INET) {
    addr_.addr4.sin_port = htons(port_);
    addr_ptr_ = reinterpret_cast<sockaddr*>(&addr_.addr4);
    addr_len_ = sizeof(addr_.addr4);
  } else if (family_ == AF_INET6) {
    addr_.addr6.sin6_port = htons(port_);
    addr_ptr_ = reinterpret_cast<sockaddr*>(&addr_.addr6);
    addr_len_ = sizeof(addr_.addr6);
  }
  return InitSocketInternal();
}

bool Socket::BindAndListen() {
  errno = 0;
  if (HANDLE_EINTR(bind(socket_, addr_ptr_, addr_len_)) < 0 ||
      HANDLE_EINTR(listen(socket_, 5)) < 0) {
    SetSocketError();
    return false;
  }
  return true;
}

bool Socket::Accept(Socket* new_socket) {
  DCHECK(new_socket != NULL);
  if (!WaitForEvent()) {
    SetSocketError();
    return false;
  }
  errno = 0;
  int new_socket_fd = HANDLE_EINTR(accept(socket_, NULL, NULL));
  if (new_socket_fd < 0) {
    SetSocketError();
    return false;
  }

  tools::DisableNagle(new_socket_fd);
  new_socket->socket_ = new_socket_fd;
  return true;
}

bool Socket::Connect() {
  errno = 0;
  if (HANDLE_EINTR(connect(socket_, addr_ptr_, addr_len_)) < 0) {
    SetSocketError();
    return false;
  }
  return true;
}

bool Socket::Resolve(const std::string& host) {
  struct addrinfo hints;
  struct addrinfo* res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;

  int errcode = getaddrinfo(host.c_str(), NULL, &hints, &res);
  if (errcode != 0) {
    SetSocketError();
    return false;
  }
  family_ = res->ai_family;
  switch (res->ai_family) {
    case AF_INET:
      memcpy(&addr_.addr4,
             reinterpret_cast<sockaddr_in*>(res->ai_addr),
             sizeof(sockaddr_in));
      break;
    case AF_INET6:
      memcpy(&addr_.addr6,
             reinterpret_cast<sockaddr_in6*>(res->ai_addr),
             sizeof(sockaddr_in6));
      break;
  }
  return true;
}

bool Socket::IsFdInSet(const fd_set& fds) const {
  if (IsClosed())
    return false;
  return FD_ISSET(socket_, &fds);
}

bool Socket::AddFdToSet(fd_set* fds) const {
  if (IsClosed())
    return false;
  FD_SET(socket_, fds);
  return true;
}

int Socket::ReadNumBytes(char* buffer, size_t num_bytes) {
  int bytes_read = 0;
  int ret = 1;
  while (bytes_read < num_bytes && ret > 0) {
    ret = Read(buffer + bytes_read, num_bytes - bytes_read);
    if (ret >= 0)
      bytes_read += ret;
  }
  return bytes_read;
}

void Socket::SetSocketError() {
  socket_error_ = true;
  // We never use non-blocking socket.
  DCHECK(errno != EAGAIN && errno != EWOULDBLOCK);
  Close();
}

int Socket::Read(char* buffer, size_t buffer_size) {
  if (!WaitForEvent()) {
    SetSocketError();
    return 0;
  }
  int ret = HANDLE_EINTR(read(socket_, buffer, buffer_size));
  if (ret < 0)
    SetSocketError();
  return ret;
}

int Socket::Write(const char* buffer, size_t count) {
  int ret = HANDLE_EINTR(send(socket_, buffer, count, MSG_NOSIGNAL));
  if (ret < 0)
    SetSocketError();
  return ret;
}

int Socket::WriteString(const std::string& buffer) {
  return WriteNumBytes(buffer.c_str(), buffer.size());
}

int Socket::WriteNumBytes(const char* buffer, size_t num_bytes) {
  int bytes_written = 0;
  int ret = 1;
  while (bytes_written < num_bytes && ret > 0) {
    ret = Write(buffer + bytes_written, num_bytes - bytes_written);
    if (ret >= 0)
      bytes_written += ret;
  }
  return bytes_written;
}

bool Socket::WaitForEvent() const {
  if (exit_notifier_fd_ == -1 || socket_ == -1)
    return true;
  const int nfds = std::max(socket_, exit_notifier_fd_) + 1;
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(socket_, &read_fds);
  FD_SET(exit_notifier_fd_, &read_fds);
  if (HANDLE_EINTR(select(nfds, &read_fds, NULL, NULL, NULL)) <= 0)
    return false;
  return !FD_ISSET(exit_notifier_fd_, &read_fds);
}

// static
int Socket::GetHighestFileDescriptor(
    const Socket& s1, const Socket& s2) {
  return std::max(s1.socket_, s2.socket_);
}

}  // namespace forwarder
