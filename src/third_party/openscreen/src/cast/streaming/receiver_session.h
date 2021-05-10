// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "cast/streaming/answer_messages.h"
#include "cast/streaming/capture_configs.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver_packet_router.h"
#include "cast/streaming/sender_message.h"
#include "cast/streaming/session_config.h"
#include "cast/streaming/session_messager.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

class Environment;
class Receiver;

class ReceiverSession final : public Environment::SocketSubscriber {
 public:
  // Upon successful negotiation, a set of configured receivers is constructed
  // for handling audio and video. Note that either receiver may be null.
  struct ConfiguredReceivers {
    // In practice, we may have 0, 1, or 2 receivers configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a receiver configuration.

    // NOTES ON LIFETIMES: The audio and video Receiver pointers are owned by
    // ReceiverSession, not the Client, and references to these pointers must be
    // cleared before a call to Client::OnReceiversDestroying() returns.

    // If the receiver is audio- or video-only, or we failed to negotiate
    // an acceptable session configuration with the sender, then either of the
    // receivers may be nullptr. In this case, the associated config is default
    // initialized and should be ignored.
    Receiver* audio_receiver;
    AudioCaptureConfig audio_config;

    Receiver* video_receiver;
    VideoCaptureConfig video_config;
  };

  // The embedder should provide a client for handling connections.
  // When a connection is established, the OnNegotiated callback is called.
  class Client {
   public:
    enum ReceiversDestroyingReason { kEndOfSession, kRenegotiated };

    // Called when a new set of receivers has been negotiated. This may be
    // called multiple times during a session, as renegotiations occur.
    virtual void OnNegotiated(const ReceiverSession* session,
                              ConfiguredReceivers receivers) = 0;

    // Called immediately preceding the destruction of this session's receivers.
    // If |reason| is |kEndOfSession|, OnNegotiated() will never be called
    // again; if it is |kRenegotiated|, OnNegotiated() will be called again
    // soon with a new set of Receivers to use.
    //
    // Before returning, the implementation must ensure that all references to
    // the Receivers, from the last call to OnNegotiated(), have been cleared.
    virtual void OnReceiversDestroying(const ReceiverSession* session,
                                       ReceiversDestroyingReason reason) = 0;

    virtual void OnError(const ReceiverSession* session, Error error) = 0;

   protected:
    virtual ~Client();
  };

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
  ReceiverSession(ReceiverSession&&) noexcept = delete;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession&&) = delete;
  ~ReceiverSession();

  const std::string& session_id() const { return session_id_; }

  // Environment::SocketSubscriber event callbacks.
  void OnSocketReady() override;
  void OnSocketInvalid(Error error) override;

 private:
  struct SessionProperties {
    std::unique_ptr<AudioStream> selected_audio;
    std::unique_ptr<VideoStream> selected_video;
    int sequence_number;

    // To be valid either the audio or video must be selected, and we must
    // have a sequence number we can reference.
    bool IsValid() const;
  };

  // Specific message type handler methods.
  void OnOffer(SenderMessage message);

  // Creates receivers and sends an appropriate Answer message using the
  // session properties.
  void InitializeSession(const SessionProperties& properties);

  // Used by SpawnReceivers to generate a receiver for a specific stream.
  std::unique_ptr<Receiver> ConstructReceiver(const Stream& stream);

  // Creates a set of configured receivers from a given pair of audio and
  // video streams. NOTE: either audio or video may be null, but not both.
  ConfiguredReceivers SpawnReceivers(const SessionProperties& properties);

  // Callers of this method should ensure at least one stream is non-null.
  Answer ConstructAnswer(const SessionProperties& properties);

  // Handles resetting receivers and notifying the client.
  void ResetReceivers(Client::ReceiversDestroyingReason reason);

  // Sends an error answer reply and notifies the client of the error.
  void SendErrorAnswerReply(int sequence_number, const char* message);

  Client* const client_;
  Environment* const environment_;
  const Preferences preferences_;
  // The sender_id of this session.
  const std::string session_id_;
  ReceiverSessionMessager messager_;

  // In some cases, the session initialization may be pending waiting for the
  // UDP socket to be ready. In this case, the receivers and the answer
  // message will not be configured and sent until the UDP socket has finished
  // binding.
  std::unique_ptr<SessionProperties> pending_session_;

  bool supports_wifi_status_reporting_ = false;
  ReceiverPacketRouter packet_router_;

  std::unique_ptr<Receiver> current_audio_receiver_;
  std::unique_ptr<Receiver> current_video_receiver_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
