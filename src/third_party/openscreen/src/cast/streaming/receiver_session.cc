// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <utility>

#include "util/logging.h"

namespace cast {
namespace streaming {

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    std::unique_ptr<Receiver> audio_receiver,
    absl::optional<SessionConfig> audio_receiver_config,
    std::unique_ptr<Receiver> video_receiver,
    absl::optional<SessionConfig> video_receiver_config)
    : audio_receiver_(std::move(audio_receiver)),
      audio_receiver_config_(std::move(audio_receiver_config)),
      video_receiver_(std::move(video_receiver)),
      video_receiver_config_(std::move(video_receiver_config)) {}

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers& ReceiverSession::ConfiguredReceivers::
operator=(ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers::~ConfiguredReceivers() = default;

ReceiverSession::ReceiverSession(Client* client, ReceiverPacketRouter* router)
    : client_(client), router_(router) {
  OSP_DCHECK(client_);
  OSP_DCHECK(router_);
}

ReceiverSession::ReceiverSession(ReceiverSession&&) noexcept = default;
ReceiverSession& ReceiverSession::operator=(ReceiverSession&&) = default;
ReceiverSession::~ReceiverSession() = default;

void ReceiverSession::SelectOffer(const SessionConfig& selected_offer) {
  // TODO(jophba): implement receiver session methods.
  OSP_UNIMPLEMENTED();
}

}  // namespace streaming
}  // namespace cast
