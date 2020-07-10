// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_
#define CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_

#include <openssl/x509.h>

#include <set>
#include <utility>
#include <vector>

#include "cast/common/channel/cast_socket.h"
#include "cast/sender/channel/cast_auth_util.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/ip_address.h"
#include "util/logging.h"

namespace cast {
namespace channel {

using openscreen::Error;
using openscreen::IPEndpoint;
using openscreen::IPEndpointComparator;
using openscreen::platform::TlsConnection;
using openscreen::platform::TlsConnectionFactory;

class SenderSocketFactory final : public TlsConnectionFactory::Client,
                                  public CastSocket::Client {
 public:
  class Client {
   public:
    virtual void OnConnected(SenderSocketFactory* factory,
                             const IPEndpoint& endpoint,
                             std::unique_ptr<CastSocket> socket) = 0;
    virtual void OnError(SenderSocketFactory* factory,
                         const IPEndpoint& endpoint,
                         Error error) = 0;
  };

  enum class DeviceMediaPolicy {
    kAudioOnly,
    kIncludesVideo,
  };

  // |client| must outlive |this|.
  explicit SenderSocketFactory(Client* client);
  ~SenderSocketFactory();

  void set_factory(TlsConnectionFactory* factory) {
    OSP_DCHECK(factory);
    factory_ = factory;
  }

  void Connect(const IPEndpoint& endpoint,
               DeviceMediaPolicy media_policy,
               CastSocket::Client* client);

  // TlsConnectionFactory::Client overrides.
  void OnAccepted(TlsConnectionFactory* factory,
                  std::vector<uint8_t> der_x509_peer_cert,
                  std::unique_ptr<TlsConnection> connection) override;
  void OnConnected(TlsConnectionFactory* factory,
                   std::vector<uint8_t> der_x509_peer_cert,
                   std::unique_ptr<TlsConnection> connection) override;
  void OnConnectionFailed(TlsConnectionFactory* factory,
                          const IPEndpoint& remote_address) override;
  void OnError(TlsConnectionFactory* factory, Error error) override;

 private:
  struct PendingConnection {
    IPEndpoint endpoint;
    DeviceMediaPolicy media_policy;
    CastSocket::Client* client;
  };

  struct PendingAuth {
    IPEndpoint endpoint;
    DeviceMediaPolicy media_policy;
    std::unique_ptr<CastSocket> socket;
    CastSocket::Client* client;
    AuthContext auth_context;
    bssl::UniquePtr<X509> peer_cert;
  };

  friend bool operator<(const std::unique_ptr<PendingAuth>& a, uint32_t b);
  friend bool operator<(uint32_t a, const std::unique_ptr<PendingAuth>& b);

  std::vector<PendingConnection>::iterator FindPendingConnection(
      const IPEndpoint& endpoint);

  // CastSocket::Client overrides.
  void OnError(CastSocket* socket, Error error) override;
  void OnMessage(CastSocket* socket, CastMessage message) override;

  Client* const client_;
  TlsConnectionFactory* factory_ = nullptr;
  std::vector<PendingConnection> pending_connections_;
  std::vector<std::unique_ptr<PendingAuth>> pending_auth_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_
