// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <vector>

#include "cast/streaming/receiver.h"
#include "cast/streaming/receiver_packet_router.h"
#include "cast/streaming/session_config.h"

namespace cast {
namespace streaming {

class ReceiverSession {
 public:
  class ConfiguredReceivers {
   public:
    // In practice, we may have 0, 1, or 2 receivers configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a receiver configuration.
    ConfiguredReceivers(
        std::unique_ptr<Receiver> audio_receiver,
        const absl::optional<SessionConfig> audio_receiver_config,
        std::unique_ptr<Receiver> video_receiver,
        const absl::optional<SessionConfig> video_receiver_config);
    ConfiguredReceivers(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers(ConfiguredReceivers&&) noexcept;
    ConfiguredReceivers& operator=(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers& operator=(ConfiguredReceivers&&) noexcept;
    ~ConfiguredReceivers();

    // If the receiver is audio- or video-only, either of the receivers
    // may be nullptr. However, in the majority of cases they will be populated.
    Receiver* audio_receiver() const { return audio_receiver_.get(); }
    const absl::optional<SessionConfig>& audio_session_config() const {
      return audio_receiver_config_;
    }
    Receiver* video_receiver() const { return video_receiver_.get(); }
    const absl::optional<SessionConfig>& video_session_config() const {
      return video_receiver_config_;
    }

   private:
    std::unique_ptr<Receiver> audio_receiver_;
    absl::optional<SessionConfig> audio_receiver_config_;
    std::unique_ptr<Receiver> video_receiver_;
    absl::optional<SessionConfig> video_receiver_config_;
  };

  class Client {
   public:
    virtual ~Client() = 0;
    virtual void OnOffer(std::vector<SessionConfig> offers) = 0;
    virtual void OnNegotiated(ConfiguredReceivers receivers) = 0;
  };

  ReceiverSession(Client* client, ReceiverPacketRouter* router);
  ReceiverSession(const ReceiverSession&) = delete;
  ReceiverSession(ReceiverSession&&) noexcept;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession&&) noexcept;
  ~ReceiverSession();

  void SelectOffer(const SessionConfig& selected_offer);

 private:
  Client* client_;
  ReceiverPacketRouter* router_;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
