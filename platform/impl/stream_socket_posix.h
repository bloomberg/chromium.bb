// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
#define PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_

#include <atomic>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/base/ip_address.h"
#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

class StreamSocketPosix {
 public:
  explicit StreamSocketPosix(const IPEndpoint& local_endpoint);
  StreamSocketPosix(SocketAddressPosix address, int file_descriptor);

  // StreamSocketPosix is non-copyable, due to directly managing the file
  // descriptor.
  StreamSocketPosix(const StreamSocketPosix& other) = delete;
  StreamSocketPosix& operator=(const StreamSocketPosix& other) = delete;
  ~StreamSocketPosix();

  // Used by passive/server sockets to accept connection requests
  // from a client. If no socket is available, this method returns nullptr.
  // To get more information, the client may call last_error().
  std::unique_ptr<StreamSocketPosix> Accept();

  // Bind to the address given in the constructor.
  Error Bind();

  // Closes the socket.
  Error Close();

  // Connects the socket to a specified remote address.
  Error Connect(const IPEndpoint& peer_endpoint);

  // Marks the socket as passive, to receive incoming connections.
  Error Listen();
  Error Listen(int max_backlog_size);

 protected:
  const SocketAddressPosix& address() const { return address_; }
  int file_descriptor() const { return file_descriptor_.load(); }
  Error last_error() const { return last_error_code_.load(); }

 private:
  // StreamSocketPosix is lazy initialized on first usage. For simplicitly,
  // the ensure method returns a boolean of whether or not the socket was
  // initialized successfully.
  bool EnsureInitialized();
  Error Initialize();

  Error CloseOnError(Error::Code error_code);
  bool IsOpen();
  Error ReportSocketClosedError();

  const SocketAddressPosix address_;
  std::atomic_int file_descriptor_;

  // last_error_code_ is an Error::Code due to atomic's type requirements.
  std::atomic<Error::Code> last_error_code_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
