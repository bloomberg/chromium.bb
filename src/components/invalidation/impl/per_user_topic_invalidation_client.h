// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/invalidation_client.h"

namespace syncer {

class InvalidationListener;
class Logger;
class NetworkChannel;

std::unique_ptr<InvalidationClient> CreatePerUserTopicInvalidationClient(
    NetworkChannel* network,
    Logger* logger,
    InvalidationListener* listener);

class PerUserTopicInvalidationClient : public InvalidationClient {
 public:
  PerUserTopicInvalidationClient(NetworkChannel* network,
                                 Logger* logger,
                                 InvalidationListener* listener);

  ~PerUserTopicInvalidationClient() override;

  /* Returns true iff the client is currently started. */
  bool IsStartedForTest() { return ticl_protocol_started_; }

  //  InvalidationClient implementation
  void Start() override;
  void Stop() override;

 private:
  InvalidationListener* GetListener() { return listener_; }

  /* Finish starting the ticl and inform the listener that it is ready. */
  void FinishStartingTiclAndInformListener();

  /* Registers a message receiver */
  void RegisterWithNetwork();

  /* Handles inbound messages from the network. */
  void MessageReceiver(const std::string& payload,
                       const std::string& private_topic,
                       const std::string& public_topic,
                       const std::string& version);
  /* Handles registartion tokens from the network. */
  void TokenReceiver(const std::string& token);

  /* The state of the Ticl whether it has started or not. */
  bool ticl_protocol_started_ = false;

  /* Resources for the Ticl. Owned by interface user */
  NetworkChannel* network_;
  Logger* logger_;
  InvalidationListener* listener_;

  base::WeakPtrFactory<PerUserTopicInvalidationClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicInvalidationClient);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_
