// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_SOCKET_H_
#define TOOLS_ANDROID_FORWARDER2_SOCKET_H_

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string>

#include "base/basictypes.h"

namespace forwarder2 {

// Wrapper class around unix socket api.  Can be used to create, bind or
// connect to both Unix domain sockets and TCP sockets.
class Socket {
 public:
  Socket();
  ~Socket();

  bool BindUnix(const std::string& path, bool abstract);
  bool BindTcp(const std::string& host, int port);
  bool ConnectUnix(const std::string& path, bool abstract);
  bool ConnectTcp(const std::string& host, int port);

  // Just a wrapper around unix socket shutdown(), see man 2 shutdown.
  void Shutdown();

  // Just a wrapper around unix socket close(), see man 2 close.
  void Close();
  bool IsClosed() const { return socket_ < 0; }

  bool Accept(Socket* new_socket);

  bool IsFdInSet(const fd_set& fds) const;
  bool AddFdToSet(fd_set* fds) const;

  // Just a wrapper around unix read() function.
  // Reads up to buffer_size, but may read less then buffer_size.
  // Returns the number of bytes read.
  int Read(char* buffer, size_t buffer_size);

  // Same as Read(), just a wrapper around write().
  int Write(const char* buffer, size_t count);

  // Calls Read() multiple times until num_bytes is written to the provided
  // buffer. No bounds checking is performed.
  // Returns number of bytes read, which can be different from num_bytes in case
  // of errror.
  int ReadNumBytes(char* buffer, size_t num_bytes);

  // Calls Write() multiple times until num_bytes is written. No bounds checking
  // is performed. Returns number of bytes written, which can be different from
  // num_bytes in case of errror.
  int WriteNumBytes(const char* buffer, size_t num_bytes);

  // Calls WriteNumBytes for the given std::string.
  int WriteString(const std::string& buffer);

  bool has_error() const { return socket_error_; }

  // |exit_notifier_fd| must be a valid pipe file descriptor created from the
  // PipeNotifier and must live (not be closed) at least as long as this socket
  // is alive.
  void set_exit_notifier_fd(int exit_notifier_fd) {
    exit_notifier_fd_ = exit_notifier_fd;
  }
  // Unset the |exit_notifier_fd_| so that it will not receive notifications
  // anymore.
  void reset_exit_notifier_fd() { exit_notifier_fd_ = -1; }

  static int GetHighestFileDescriptor(const Socket& s1, const Socket& s2);

 private:
  union SockAddr {
    // IPv4 sockaddr
    sockaddr_in addr4;
    // IPv6 sockaddr
    sockaddr_in6 addr6;
    // Unix Domain sockaddr
    sockaddr_un addr_un;
  };

  // If |host| is empty, use localhost.
  bool InitTcpSocket(const std::string& host, int port);
  bool InitUnixSocket(const std::string& path, bool abstract);
  bool BindAndListen();
  bool Connect();

  bool Resolve(const std::string& host);
  bool InitSocketInternal();
  void SetSocketError();

  // Waits until either the Socket or the |exit_notifier_fd_| has received a
  // read event (accept or read). Returns false iff an exit notification was
  // received.
  bool WaitForEvent() const;

  int socket_;
  int port_;
  bool socket_error_;

  // Family of the socket (PF_INET, PF_INET6 or PF_UNIX).
  int family_;

  // True if this is an abstract unix domain socket.
  bool abstract_;

  SockAddr addr_;

  // Points to one of the members of the above union depending on the family.
  sockaddr* addr_ptr_;
  // Length of one of the members of the above union depending on the family.
  socklen_t addr_len_;

  // File descriptor from PipeNotifier (see pipe_notifier.h) to send application
  // exit notifications before calling socket blocking operations such as Read
  // and Accept.
  int exit_notifier_fd_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

}  // namespace forwarder

#endif  // TOOLS_ANDROID_FORWARDER2_SOCKET_H_
