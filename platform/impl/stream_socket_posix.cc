// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/stream_socket_posix.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

namespace {
constexpr int kDefaultMaxBacklogSize = 64;
constexpr int kInvalidFileDescriptor = -1;
}  // namespace

StreamSocketPosix::StreamSocketPosix(const IPEndpoint& local_endpoint)
    : address_(local_endpoint),
      file_descriptor_(kInvalidFileDescriptor),
      last_error_code_(Error::Code::kNone) {}

StreamSocketPosix::StreamSocketPosix(SocketAddressPosix address,
                                     int file_descriptor)
    : address_(address),
      file_descriptor_(file_descriptor),
      last_error_code_(Error::Code::kNone) {}

StreamSocketPosix::~StreamSocketPosix() {
  if (IsOpen()) {
    Close();
  }
}

std::unique_ptr<StreamSocketPosix> StreamSocketPosix::Accept() {
  if (!EnsureInitialized()) {
    ReportSocketClosedError();
    return nullptr;
  }

  // We copy our address to new_peer_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_peer_address = address_;
  socklen_t peer_address_size = new_peer_address.size();
  const int new_file_descriptor = accept(
      file_descriptor_.load(), new_peer_address.address(), &peer_address_size);
  if (new_file_descriptor == kInvalidFileDescriptor) {
    CloseOnError(Error::Code::kSocketAcceptFailure);
    return nullptr;
  }

  return std::make_unique<StreamSocketPosix>(new_peer_address,
                                             new_file_descriptor);
}

Error StreamSocketPosix::Bind() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (bind(file_descriptor_.load(), address_.address(), address_.size()) != 0) {
    return CloseOnError(Error::Code::kSocketBindFailure);
  }

  return Error::None();
}

Error StreamSocketPosix::Close() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  const int file_descriptor_to_close =
      file_descriptor_.exchange(kInvalidFileDescriptor);
  if (close(file_descriptor_to_close) != 0) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  return Error::None();
}

Error StreamSocketPosix::Connect(const IPEndpoint& peer_endpoint) {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  SocketAddressPosix address(peer_endpoint);
  if (connect(file_descriptor_.load(), address.address(), address.size()) !=
      0) {
    return CloseOnError(Error::Code::kSocketConnectFailure);
  }

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

bool StreamSocketPosix::EnsureInitialized() {
  if (!IsOpen() && (last_error_code_.load() == Error::Code::kNone)) {
    return Initialize() == Error::None();
  }

  return false;
}

Error StreamSocketPosix::Initialize() {
  if (IsOpen()) {
    return Error::Code::kItemAlreadyExists;
  }

  int domain;
  switch (address_.version()) {
    case IPAddress::Version::kV4:
      domain = AF_INET;
      break;
    case IPAddress::Version::kV6:
      domain = AF_INET6;
      break;
  }

  const int new_file_descriptor = socket(domain, SOCK_STREAM, 0);
  if (new_file_descriptor == kInvalidFileDescriptor) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  const int current_flags = fcntl(new_file_descriptor, F_GETFL, 0);
  if (fcntl(new_file_descriptor, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(new_file_descriptor);
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  file_descriptor_ = new_file_descriptor;
  // last_error_code_ should still be Error::None().
  return Error::None();
}

Error StreamSocketPosix::CloseOnError(Error::Code error_code) {
  last_error_code_ = error_code;
  Close();
  return error_code;
}

// If is_open is false, the socket has either not been initialized
// or has been closed, either on purpose or due to error.
Error StreamSocketPosix::ReportSocketClosedError() {
  last_error_code_ = Error::Code::kSocketClosedFailure;
  return Error::Code::kSocketClosedFailure;
}

// The file_descriptor_ field is initialized to an invalid value, and resets
// to that value when closed. We can assume that if the file_descriptor_ is
// set, we are open.
bool StreamSocketPosix::IsOpen() {
  return file_descriptor_ != kInvalidFileDescriptor;
}
}  // namespace platform
}  // namespace openscreen
