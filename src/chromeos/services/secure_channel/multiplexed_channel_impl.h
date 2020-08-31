// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_IMPL_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/authenticated_channel.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/multiplexed_channel.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/single_client_message_proxy.h"

namespace chromeos {

namespace secure_channel {

// Concrete MultiplexedChannel, which uses an AuthenticatedChannel for its
// underlying communication channel. Each client added to the channel is tracked
// via a SingleClientMessageProxy.
//
// Since a MultiplexedChannel should only be active for one or more clients, all
// MultiplexedChannelImpl objects must be created via the provided Factory
// class, which verifies that at least one initial client is provided.
class MultiplexedChannelImpl : public MultiplexedChannel,
                               public AuthenticatedChannel::Observer,
                               public SingleClientMessageProxy::Delegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<MultiplexedChannel> Create(
        std::unique_ptr<AuthenticatedChannel> authenticated_channel,
        MultiplexedChannel::Delegate* delegate,
        ConnectionDetails connection_details,
        std::vector<std::unique_ptr<ClientConnectionParameters>>*
            initial_clients);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<MultiplexedChannel> CreateInstance(
        std::unique_ptr<AuthenticatedChannel> authenticated_channel,
        MultiplexedChannel::Delegate* delegate,
        ConnectionDetails connection_details,
        std::vector<std::unique_ptr<ClientConnectionParameters>>*
            initial_clients) = 0;

   private:
    static Factory* test_factory_;
  };

  ~MultiplexedChannelImpl() override;

 private:
  MultiplexedChannelImpl(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      MultiplexedChannel::Delegate* delegate,
      ConnectionDetails connection_details);

  // MultiplexedChannel:
  bool IsDisconnecting() const override;
  bool IsDisconnected() const override;
  void PerformAddClientToChannel(std::unique_ptr<ClientConnectionParameters>
                                     client_connection_parameters) override;

  // AuthenticatedChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& feature,
                         const std::string& payload) override;

  // SingleClientMessageProxy::Delegate:
  void OnSendMessageRequested(const std::string& message_feaure,
                              const std::string& message_payload,
                              base::OnceClosure on_sent_callback) override;
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void OnClientDisconnected(const base::UnguessableToken& proxy_id) override;

  std::unique_ptr<AuthenticatedChannel> authenticated_channel_;

  bool is_disconnecting_ = false;
  bool is_disconnected_ = false;

  std::unordered_map<base::UnguessableToken,
                     std::unique_ptr<SingleClientMessageProxy>,
                     base::UnguessableTokenHash>
      id_to_proxy_map_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexedChannelImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_MULTIPLEXED_CHANNEL_IMPL_H_
