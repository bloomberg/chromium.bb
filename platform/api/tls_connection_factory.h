// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CONNECTION_FACTORY_H_
#define PLATFORM_API_TLS_CONNECTION_FACTORY_H_

#include <openssl/crypto.h>

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/api/tls_connection.h"
#include "platform/base/ip_address.h"
#include "platform/base/tls_connect_options.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"

namespace openscreen {
namespace platform {

// We expect a single factory to be able to handle an arbitrary number of
// calls using the same client and task runner.
class TlsConnectionFactory {
 public:
  // Client callbacks are ran on the provided TaskRunner.
  class Client {
   public:
    virtual void OnAccepted(TlsConnectionFactory* factory,
                            std::unique_ptr<TlsConnection> connection) = 0;

    virtual void OnConnected(TlsConnectionFactory* factory,
                             std::unique_ptr<TlsConnection> connection) = 0;

    virtual void OnConnectionFailed(TlsConnectionFactory* factory,
                                    const IPEndpoint& remote_address) = 0;

    // It means it's so bad that you might as well delete the factory.
    virtual void OnError(TlsConnectionFactory* factory, Error error) = 0;
  };

  // The connection factory requires a client for yielding creation results
  // asynchronously, as well as a task runner it can use to for running
  // callbacks both on the factory and on created TlsConnection instances.
  static std::unique_ptr<TlsConnectionFactory> CreateFactory(
      Client* client,
      TaskRunner* task_runner);

  virtual ~TlsConnectionFactory() = default;

  // TODO(jophba, rwkeane): Determine how to handle multiple connection attempts
  // to the same remote_address, and how to distinguish errors.
  // Fires an OnConnected or OnConnectionFailed event.
  virtual void Connect(const IPEndpoint& remote_address,
                       const TlsConnectOptions& options) = 0;

  // Fires an OnAccepted or OnConnectionFailed event.
  virtual void Listen(const IPEndpoint& local_address,
                      const TlsCredentials& credentials,
                      const TlsListenOptions& options) = 0;

 protected:
  TlsConnectionFactory(Client* client, TaskRunner* task_runner)
      : client_(client), task_runner_(task_runner) {}

  // TODO(jophba, rwkeane): Hide access to these behind local methods.
  Client* client() const { return client_; }
  TaskRunner* task_runner() const { return task_runner_; }

 private:
  Client* client_;
  TaskRunner* task_runner_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_FACTORY_H_
