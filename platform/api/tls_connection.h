// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CONNECTION_H_
#define PLATFORM_API_TLS_CONNECTION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "platform/api/network_interface.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsConnection {
 public:
  // Client callbacks are ran on the provided TaskRunner.
  class Client {
   public:
    // Called when |connection| writing is blocked and unblocked, respectively.
    // Note that implementations should do best effort to buffer packets even in
    // blocked state, and should call OnError if we actually overflow the
    // buffer.
    virtual void OnWriteBlocked(TlsConnection* connection) = 0;
    virtual void OnWriteUnblocked(TlsConnection* connection) = 0;

    // Called when |connection| experiences an error, such as a read error.
    virtual void OnError(TlsConnection* connection, Error error) = 0;

    // Called when a |block| arrives on |connection|.
    virtual void OnRead(TlsConnection* connection,
                        std::vector<uint8_t> block) = 0;

   protected:
    virtual ~Client() = default;
  };

  // Sends a message.
  virtual void Write(const void* data, size_t len) = 0;

  // Get the local address.
  virtual IPEndpoint local_address() const = 0;

  // Get the connected remote address.
  virtual IPEndpoint remote_address() const = 0;

  // Sets the client for this instance.
  void set_client(Client* client) { client_ = client; }

  virtual ~TlsConnection() = default;

 protected:
  explicit TlsConnection(TaskRunner* task_runner) : task_runner_(task_runner) {}

  // Called when |connection| writing is blocked and unblocked, respectively.
  // This call will be proxied to the Client set for this TlsConnection.
  void OnWriteBlocked();
  void OnWriteUnblocked();

  // Called when |connection| experiences an error, such as a read error. This
  // call will be proxied to the Client set for this TlsConnection.
  void OnError(Error error);

  // Called when a |packet| arrives on |socket|. This call will be proxied to
  // the Client set for this TlsConnection.
  void OnRead(std::vector<uint8_t> message);

 private:
  Client* client_;
  TaskRunner* const task_runner_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsConnection);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_H_
