// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/active_connection_manager_impl.h"

#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/multiplexed_channel_impl.h"

namespace chromeos {

namespace secure_channel {

// static
ActiveConnectionManagerImpl::Factory*
    ActiveConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
ActiveConnectionManagerImpl::Factory*
ActiveConnectionManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<ActiveConnectionManagerImpl::Factory> factory;
  return factory.get();
}

// static
void ActiveConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ActiveConnectionManagerImpl::Factory::~Factory() = default;

std::unique_ptr<ActiveConnectionManager>
ActiveConnectionManagerImpl::Factory::BuildInstance(
    ActiveConnectionManager::Delegate* delegate) {
  return base::WrapUnique(new ActiveConnectionManagerImpl(delegate));
}

ActiveConnectionManagerImpl::ActiveConnectionManagerImpl(
    ActiveConnectionManager::Delegate* delegate)
    : ActiveConnectionManager(delegate) {}

ActiveConnectionManagerImpl::~ActiveConnectionManagerImpl() = default;

ActiveConnectionManager::ConnectionState
ActiveConnectionManagerImpl::GetConnectionState(
    const ConnectionDetails& connection_details) const {
  auto it = details_to_channel_map_.find(connection_details);
  if (it == details_to_channel_map_.end())
    return ConnectionState::kNoConnectionExists;

  const MultiplexedChannel* channel = it->second.get();
  DCHECK(channel);
  DCHECK(!channel->IsDisconnected());

  return channel->IsDisconnecting()
             ? ConnectionState::kDisconnectingConnectionExists
             : ConnectionState::kActiveConnectionExists;
}

void ActiveConnectionManagerImpl::PerformAddActiveConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
    const ConnectionDetails& connection_details) {
  details_to_channel_map_[connection_details] =
      MultiplexedChannelImpl::Factory::Get()->BuildInstance(
          std::move(authenticated_channel), this /* delegate */,
          connection_details, &initial_clients);
}

void ActiveConnectionManagerImpl::PerformAddClientToChannel(
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    const ConnectionDetails& connection_details) {
  bool success =
      details_to_channel_map_[connection_details]->AddClientToChannel(
          std::move(client_connection_parameters));
  if (!success) {
    PA_LOG(ERROR) << "ActiveConnectionManagerImpl::"
                  << "PerformAddClientToChannel(): Could not add "
                  << "ClientConnectionParameters to MultiplexedChannel.";
    NOTREACHED();
  }
}

void ActiveConnectionManagerImpl::OnDisconnected(
    const ConnectionDetails& connection_details) {
  // Make a copy of |connection_details|, since the owner of these details is
  // deleted below.
  const ConnectionDetails connection_details_copy = connection_details;

  size_t num_deleted = details_to_channel_map_.erase(connection_details);
  if (num_deleted != 1u) {
    PA_LOG(ERROR) << "ActiveConnectionManagerImpl::OnDisconnected(): Tried to "
                  << "delete map entry, but it did not exist.";
    NOTREACHED();
  }

  OnChannelDisconnected(connection_details_copy);
}

}  // namespace secure_channel

}  // namespace chromeos
