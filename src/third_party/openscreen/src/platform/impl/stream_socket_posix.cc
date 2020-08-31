// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/stream_socket_posix.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace openscreen {

namespace {
constexpr int kDefaultMaxBacklogSize = 64;

// Call Select with no timeout, so that it doesn't block. Then use the result
// to determine if any connection is pending.
bool IsConnectionPending(int fd) {
  fd_set handle_set;
  FD_ZERO(&handle_set);
  FD_SET(fd, &handle_set);
  struct timeval tv {
    0
  };
  return select(fd + 1, &handle_set, nullptr, nullptr, &tv) > 0;
}
}  // namespace

StreamSocketPosix::StreamSocketPosix(IPAddress::Version version)
    : version_(version) {}

StreamSocketPosix::StreamSocketPosix(const IPEndpoint& local_endpoint)
    : version_(local_endpoint.address.version()),
      local_address_(local_endpoint) {}

StreamSocketPosix::StreamSocketPosix(SocketAddressPosix local_address,
                                     IPEndpoint remote_address,
                                     int file_descriptor)
    : handle_(file_descriptor),
      version_(local_address.version()),
      local_address_(local_address),
      remote_address_(remote_address),
      state_(SocketState::kConnected) {
  EnsureInitialized();
}

StreamSocketPosix::~StreamSocketPosix() {
  if (state_ == SocketState::kConnected) {
    Close();
  }
}

WeakPtr<StreamSocketPosix> StreamSocketPosix::GetWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

ErrorOr<std::unique_ptr<StreamSocket>> StreamSocketPosix::Accept() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (!is_bound_) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  // Check if any connection is pending, and return a special error code if not.
  if (!IsConnectionPending(handle_.fd)) {
    return Error::Code::kAgain;
  }

  // We copy our address to new_remote_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_remote_address = local_address_.value();
  socklen_t remote_address_size = new_remote_address.size();
  const int new_file_descriptor =
      accept(handle_.fd, new_remote_address.address(), &remote_address_size);
  if (new_file_descriptor == kUnsetHandleFd) {
    return CloseOnError(
        Error(Error::Code::kSocketAcceptFailure, strerror(errno)));
  }
  new_remote_address.RecomputeEndpoint();

  return ErrorOr<std::unique_ptr<StreamSocket>>(
      std::make_unique<StreamSocketPosix>(local_address_.value(),
                                          new_remote_address.endpoint(),
                                          new_file_descriptor));
}

Error StreamSocketPosix::Bind() {
  if (!local_address_.has_value()) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (is_bound_) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  if (bind(handle_.fd, local_address_.value().address(),
           local_address_.value().size()) != 0) {
    return CloseOnError(
        Error(Error::Code::kSocketBindFailure, strerror(errno)));
  }

  is_bound_ = true;
  return Error::None();
}

Error StreamSocketPosix::Close() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (state_ == SocketState::kClosed) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  const int file_descriptor_to_close = handle_.fd;
  if (close(file_descriptor_to_close) != 0) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }
  handle_.fd = kUnsetHandleFd;

  return Error::None();
}

Error StreamSocketPosix::Connect(const IPEndpoint& remote_endpoint) {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (!is_initialized_ && !is_bound_) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  SocketAddressPosix address(remote_endpoint);
  int ret = connect(handle_.fd, address.address(), address.size());
  if (ret != 0 && errno != EINPROGRESS) {
    return CloseOnError(
        Error(Error::Code::kSocketConnectFailure, strerror(errno)));
  }

  if (!is_bound_) {
    if (local_address_.has_value()) {
      return CloseOnError(Error::Code::kSocketInvalidState);
    }

    struct sockaddr_in6 address;
    socklen_t size = sizeof(address);
    if (getsockname(handle_.fd, reinterpret_cast<struct sockaddr*>(&address),
                    &size) != 0) {
      return CloseOnError(Error::Code::kSocketConnectFailure);
    }

    local_address_.emplace(reinterpret_cast<struct sockaddr&>(address));
    is_bound_ = true;
  }

  remote_address_ = remote_endpoint;
  state_ = SocketState::kConnected;
  return Error::None();
}

Error StreamSocketPosix::Listen() {
  return Listen(kDefaultMaxBacklogSize);
}

Error StreamSocketPosix::Listen(int max_backlog_size) {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (listen(handle_.fd, max_backlog_size) != 0) {
    return CloseOnError(
        Error(Error::Code::kSocketListenFailure, strerror(errno)));
  }

  return Error::None();
}

absl::optional<IPEndpoint> StreamSocketPosix::remote_address() const {
  if ((state_ != SocketState::kConnected) || !remote_address_) {
    return absl::nullopt;
  }
  return remote_address_.value();
}

absl::optional<IPEndpoint> StreamSocketPosix::local_address() const {
  if (!local_address_.has_value()) {
    return absl::nullopt;
  }
  return local_address_.value().endpoint();
}

SocketState StreamSocketPosix::state() const {
  return state_;
}

IPAddress::Version StreamSocketPosix::version() const {
  return version_;
}

bool StreamSocketPosix::EnsureInitialized() {
  if (!is_initialized_ && (last_error_code_ == Error::Code::kNone)) {
    return Initialize() == Error::None();
  }

  return handle_.fd != kUnsetHandleFd && is_initialized_;
}

Error StreamSocketPosix::Initialize() {
  if (is_initialized_) {
    return Error::Code::kItemAlreadyExists;
  }

  int fd = handle_.fd;
  if (fd == kUnsetHandleFd) {
    int domain;
    switch (version_) {
      case IPAddress::Version::kV4:
        domain = AF_INET;
        break;
      case IPAddress::Version::kV6:
        domain = AF_INET6;
        break;
    }

    fd = socket(domain, SOCK_STREAM, 0);
    if (fd == kUnsetHandleFd) {
      last_error_code_ = Error::Code::kSocketInvalidState;
      return Error::Code::kSocketInvalidState;
    }
  }

  const int current_flags = fcntl(fd, F_GETFL, 0);
  if (fcntl(fd, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(fd);
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  handle_.fd = fd;
  is_initialized_ = true;
  // last_error_code_ should still be Error::None().
  return Error::None();
}

Error StreamSocketPosix::CloseOnError(Error error) {
  last_error_code_ = error.code();
  Close();
  state_ = SocketState::kClosed;
  return error;
}

// If is_open is false, the socket has either not been initialized
// or has been closed, either on purpose or due to error.
Error StreamSocketPosix::ReportSocketClosedError() {
  last_error_code_ = Error::Code::kSocketClosedFailure;
  return Error::Code::kSocketClosedFailure;
}
}  // namespace openscreen
