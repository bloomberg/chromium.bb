// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_invalidation_client.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/logger.h"
#include "components/invalidation/impl/network_channel.h"

namespace {

const char kPrivatTopicName[] = "/topics";
const int kPrivatTopicNameSize = 7;
}

namespace syncer {

std::unique_ptr<InvalidationClient> CreatePerUserTopicInvalidationClient(
    NetworkChannel* network,
    Logger* logger,
    InvalidationListener* listener) {
  return std::make_unique<PerUserTopicInvalidationClient>(network, logger,
                                                          listener);
}

PerUserTopicInvalidationClient::PerUserTopicInvalidationClient(
    NetworkChannel* network,
    Logger* logger,
    InvalidationListener* listener)
    : network_(network),
      logger_(logger),
      listener_(listener),
      weak_factory_(this) {
  RegisterWithNetwork();
  TLOG(logger_, INFO, "Created client");
}

PerUserTopicInvalidationClient::~PerUserTopicInvalidationClient() {}

void PerUserTopicInvalidationClient::RegisterWithNetwork() {
  // Install ourselves as a receiver for server messages.
  network_->SetMessageReceiver(
      base::BindRepeating(&PerUserTopicInvalidationClient::MessageReceiver,
                          weak_factory_.GetWeakPtr()));
  network_->SetTokenReceiver(
      base::BindRepeating(&PerUserTopicInvalidationClient::TokenReceiver,
                          weak_factory_.GetWeakPtr()));
}

void PerUserTopicInvalidationClient::Start() {
  if (ticl_protocol_started_) {
    TLOG(logger_, SEVERE, "Ignoring start call since already started");
    return;
  }
  FinishStartingTiclAndInformListener();
}

void PerUserTopicInvalidationClient::Stop() {
  TLOG(logger_, INFO, "Ticl being stopped");
  ticl_protocol_started_ = false;
}

void PerUserTopicInvalidationClient::FinishStartingTiclAndInformListener() {
  DCHECK(!ticl_protocol_started_);
  ticl_protocol_started_ = true;
  GetListener()->Ready(this);
  TLOG(logger_, INFO, "Ticl started");
}

void PerUserTopicInvalidationClient::MessageReceiver(
    const std::string& payload,
    const std::string& private_topic,
    const std::string& public_topic,
    const std::string& version) {
  std::string private_topic_name = private_topic;
  if (base::StartsWith(private_topic, kPrivatTopicName,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // FCM protocol requires topic to start with"/topics" to topic name.
    // Reason why it is necessary to strip the prefix is that later the shorter
    // topic name is used for indexing into maps.
    private_topic_name = private_topic.substr(kPrivatTopicNameSize);
  }
  int64_t v = 0;
  if (!base::StringToInt64(version, &v)) {
    // Version must always be in the message and
    // in addition version must be number.
    // TODO(melandory): Report Error to listener;
  }
  GetListener()->Invalidate(this, payload, private_topic_name, public_topic, v);
}

void PerUserTopicInvalidationClient::TokenReceiver(const std::string& token) {
  GetListener()->InformTokenRecieved(this, token);
  if (!ticl_protocol_started_)
    FinishStartingTiclAndInformListener();
}

}  // namespace invalidation
