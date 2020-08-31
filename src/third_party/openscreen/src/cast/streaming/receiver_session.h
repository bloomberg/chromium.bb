// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "cast/streaming/answer_messages.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver_packet_router.h"
#include "cast/streaming/session_config.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

class CastSocket;
class Environment;
class Receiver;
class VirtualConnectionRouter;
class VirtualConnection;

class ReceiverSession final : public MessagePort::Client {
 public:
  // A small helper struct that contains all of the information necessary for
  // a configured receiver, including a receiver, its session config, and the
  // stream selected from the OFFER message to instantiate the receiver.
  template <typename T>
  struct ConfiguredReceiver {
    Receiver* receiver;
    const SessionConfig receiver_config;
    const T selected_stream;
  };

  // Upon successful negotiation, a set of configured receivers is constructed
  // for handling audio and video. Note that either receiver may be null.
  struct ConfiguredReceivers {
    // In practice, we may have 0, 1, or 2 receivers configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a receiver configuration.

    // NOTES ON LIFETIMES: The audio and video receiver pointers are expected
    // to be valid until the OnConfiguredReceiversDestroyed event is fired, at
    // which point they become invalid and need to replaced by the results of
    // the ensuing OnNegotiated call.

    // If the receiver is audio- or video-only, either of the receivers
    // may be nullptr. However, in the majority of cases they will be populated.
    absl::optional<ConfiguredReceiver<AudioStream>> audio;
    absl::optional<ConfiguredReceiver<VideoStream>> video;
  };

  // The embedder should provide a client for handling connections.
  // When a connection is established, the OnNegotiated callback is called.
  class Client {
   public:
    // This method is called when a new set of receivers has been negotiated.
    virtual void OnNegotiated(const ReceiverSession* session,
                              ConfiguredReceivers receivers) = 0;

    // This method is called immediately preceding the invalidation of
    // this session's receivers.
    virtual void OnConfiguredReceiversDestroyed(
        const ReceiverSession* session) = 0;

    virtual void OnError(const ReceiverSession* session, Error error) = 0;
  };

  // The embedder has the option of providing a list of prioritized
  // preferences for selecting from the offer.
  enum class AudioCodec : int { kAac, kOpus };
  enum class VideoCodec : int { kH264, kVp8, kHevc, kVp9 };

  // Note: embedders are required to implement the following
  // codecs to be Cast V2 compliant: H264, VP8, AAC, Opus.
  struct Preferences {
    Preferences();
    Preferences(std::vector<VideoCodec> video_codecs,
                std::vector<AudioCodec> audio_codecs);
    Preferences(std::vector<VideoCodec> video_codecs,
                std::vector<AudioCodec> audio_codecs,
                std::unique_ptr<Constraints> constraints,
                std::unique_ptr<DisplayDescription> description);

    Preferences(Preferences&&) noexcept;
    Preferences(const Preferences&) = delete;
    Preferences& operator=(Preferences&&) noexcept;
    Preferences& operator=(const Preferences&) = delete;

    std::vector<VideoCodec> video_codecs{VideoCodec::kVp8, VideoCodec::kH264};
    std::vector<AudioCodec> audio_codecs{AudioCodec::kOpus, AudioCodec::kAac};

    // The embedder has the option of directly specifying the display
    // information and video/audio constraints that will be passed along to
    // senders during the offer/answer exchange. If nullptr, these are ignored.
    std::unique_ptr<Constraints> constraints;
    std::unique_ptr<DisplayDescription> display_description;
  };

  ReceiverSession(Client* const client,
                  Environment* environment,
                  MessagePort* message_port,
                  Preferences preferences);
  ReceiverSession(const ReceiverSession&) = delete;
  ReceiverSession(ReceiverSession&&) = delete;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession&&) = delete;
  ~ReceiverSession();

  // MessagePort::Client overrides
  void OnMessage(absl::string_view sender_id,
                 absl::string_view message_namespace,
                 absl::string_view message) override;
  void OnError(Error error) override;

 private:
  struct Message {
    const std::string sender_id = {};
    const std::string message_namespace = {};
    const int sequence_number = 0;
    Json::Value body;
  };

  // Message handlers
  void OnOffer(Message* message);

  std::pair<SessionConfig, std::unique_ptr<Receiver>> ConstructReceiver(
      const Stream& stream);

  // Either stream input to this method may be null, however if both
  // are null this method returns error.
  ErrorOr<ConfiguredReceivers> TrySpawningReceivers(const AudioStream* audio,
                                                    const VideoStream* video);

  // Callers of this method should ensure at least one stream is non-null.
  Answer ConstructAnswer(Message* message,
                         const AudioStream* audio,
                         const VideoStream* video);

  void SendMessage(Message* message);

  // Handles resetting receivers and notifying the client.
  void ResetReceivers();

  Client* const client_;
  Environment* const environment_;
  MessagePort* const message_port_;
  const Preferences preferences_;

  CastMode cast_mode_;
  bool supports_wifi_status_reporting_ = false;
  ReceiverPacketRouter packet_router_;

  std::unique_ptr<Receiver> current_audio_receiver_;
  std::unique_ptr<Receiver> current_video_receiver_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
