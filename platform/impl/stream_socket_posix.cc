// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/stream_socket_posix.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace openscreen {
namespace platform {

namespace {
constexpr int kDefaultMaxBacklogSize = 64;
}  // namespace

StreamSocketPosix::StreamSocketPosix(IPAddress::Version version)
    : version_(version) {}

StreamSocketPosix::StreamSocketPosix(const IPEndpoint& local_endpoint)
    : version_(local_endpoint.address.version()),
      local_address_(local_endpoint) {}

StreamSocketPosix::StreamSocketPosix(SocketAddressPosix local_address,
                                     int file_descriptor)
    : file_descriptor_(file_descriptor),
      version_(local_address.version()),
      local_address_(local_address) {}

StreamSocketPosix::~StreamSocketPosix() {
  if (state_ == SocketState::kConnected) {
    Close();
  }
}

ErrorOr<std::unique_ptr<StreamSocket>> StreamSocketPosix::Accept() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (!is_bound_) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  // We copy our address to new_remote_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_remote_address = local_address_.value();
  socklen_t remote_address_size = new_remote_address.size();
  const int new_file_descriptor =
      accept(file_descriptor_.load(), new_remote_address.address(),
             &remote_address_size);
  if (new_file_descriptor == -1) {
    return CloseOnError(Error::Code::kSocketAcceptFailure);
  }

  return ErrorOr<std::unique_ptr<StreamSocket>>(
      std::make_unique<StreamSocketPosix>(new_remote_address,
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

  if (bind(file_descriptor_.load(), local_address_.value().address(),
           local_address_.value().size()) != 0) {
    return CloseOnError(Error::Code::kSocketBindFailure);
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

  const int file_descriptor_to_close = file_descriptor_.exchange(-1);
  if (close(file_descriptor_to_close) != 0) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

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
  if (connect(file_descriptor_.load(), address.address(), address.size()) !=
      0) {
    return CloseOnError(Error::Code::kSocketConnectFailure);
  }

  if (!is_bound_) {
    if (local_address_.has_value()) {
      return CloseOnError(Error::Code::kSocketInvalidState);
    }

    struct sockaddr address;
    socklen_t size = sizeof(address);
    if (getsockname(file_descriptor_.load(), &address, &size) != 0) {
      return CloseOnError(Error::Code::kSocketConnectFailure);
    }

    local_address_.emplace(address);
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

  if (listen(file_descriptor_.load(), max_backlog_size) != 0) {
    return CloseOnError(Error::Code::kSocketListenFailure);
  }

  return Error::None();
}

SocketHandle StreamSocketPosix::socket_handle() const {
  return SocketHandle{file_descriptor_.load()};
}

absl::optional<IPEndpoint> StreamSocketPosix::remote_address() const {
  if ((state_ != SocketState::kConnected) || !remote_address_) {
    return absl::nullopt;
  }
  return remote_address_.value();
}

SocketState StreamSocketPosix::state() const {
  return state_;
}

IPAddress::Version StreamSocketPosix::version() const {
  return version_;
}

bool StreamSocketPosix::EnsureInitialized() {
  if (!is_initialized_ && (last_error_code_.load() == Error::Code::kNone)) {
    return Initialize() == Error::None();
  }

  return false;
}

Error StreamSocketPosix::Initialize() {
  if (is_initialized_) {
    return Error::Code::kItemAlreadyExists;
  }

  int domain;
  switch (version_) {
    case IPAddress::Version::kV4:
      domain = AF_INET;
      break;
    case IPAddress::Version::kV6:
      domain = AF_INET6;
      break;
  }

  file_descriptor_ = socket(domain, SOCK_STREAM, 0);
  if (file_descriptor_ == -1) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  const int current_flags = fcntl(file_descriptor_, F_GETFL, 0);
  if (fcntl(file_descriptor_, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(file_descriptor_);
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  is_initialized_ = true;
  // last_error_code_ should still be Error::None().
  return Error::None();
}

Error StreamSocketPosix::CloseOnError(Error::Code error_code) {
  last_error_code_ = error_code;
  Close();
  state_ = SocketState::kClosed;
  return error_code;
}

// If is_open is false, the socket has either not been initialized
// or has been closed, either on purpose or due to error.
Error StreamSocketPosix::ReportSocketClosedError() {
  last_error_code_ = Error::Code::kSocketClosedFailure;
  return Error::Code::kSocketClosedFailure;
}
}  // namespace platform
}  // namespace openscreen
