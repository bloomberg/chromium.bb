// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SENDER_SESSION_H_
#define CAST_STREAMING_SENDER_SESSION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "cast/streaming/answer_messages.h"
#include "cast/streaming/capture_configs.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "cast/streaming/sender_packet_router.h"
#include "cast/streaming/session_config.h"
#include "cast/streaming/session_messager.h"
#include "json/value.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace capture_recommendations {
struct Recommendations;
}

class Environment;
class Sender;

class SenderSession final {
 public:
  // Upon successful negotiation, a set of configured senders is constructed
  // for handling audio and video. Note that either sender may be null.
  struct ConfiguredSenders {
    // In practice, we may have 0, 1, or 2 senders configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a sender configuration.

    // If the sender is audio- or video-only, either of the senders
    // may be nullptr. However, in the majority of cases they will be populated.
    Sender* audio_sender;
    AudioCaptureConfig audio_config;

    Sender* video_sender;
    VideoCaptureConfig video_config;
  };

  // The embedder should provide a client for handling the negotiation.
  // When the negotiation is complete, the OnNegotiated callback is called.
  class Client {
   public:
    // Called when a new set of senders has been negotiated. This may be
    // called multiple times during a session, once for every time Negotiate()
    // is called on the SenderSession object. The negotiation call also includes
    // capture recommendations that can be used by the sender to provide
    // an optimal video stream for the receiver.
    virtual void OnNegotiated(
        const SenderSession* session,
        ConfiguredSenders senders,
        capture_recommendations::Recommendations capture_recommendations) = 0;

    // Called whenever an error occurs. Ends the ongoing session, and the caller
    // must call Negotiate() again if they wish to re-establish streaming.
    virtual void OnError(const SenderSession* session, Error error) = 0;

   protected:
    virtual ~Client();
  };

  // The SenderSession assumes that the passed in client, environment, and
  // message port persist for at least the lifetime of the SenderSession. If
  // one of these classes needs to be reset, a new SenderSession should be
  // created.
  //
  // |message_source_id| and |message_destination_id| are the local and remote
  // ID, respectively, to use when sending or receiving control messages (e.g.,
  // OFFERs or ANSWERs) over the |message_port|. |message_port|'s SetClient()
  // method will be called.
  SenderSession(IPAddress remote_address,
                Client* const client,
                Environment* environment,
                MessagePort* message_port,
                std::string message_source_id,
                std::string message_destination_id);
  SenderSession(const SenderSession&) = delete;
  SenderSession(SenderSession&&) noexcept = delete;
  SenderSession& operator=(const SenderSession&) = delete;
  SenderSession& operator=(SenderSession&&) = delete;
  ~SenderSession();

  // Starts an OFFER/ANSWER exchange with the already configured receiver
  // over the message port. The caller should assume any configured senders
  // become invalid when calling this method.
  Error Negotiate(std::vector<AudioCaptureConfig> audio_configs,
                  std::vector<VideoCaptureConfig> video_configs);

 private:
  // We store the current negotiation, so that when we get an answer from the
  // receiver we can line up the selected streams with the original
  // configuration.
  struct Negotiation {
    Offer offer;

    std::vector<AudioCaptureConfig> audio_configs;
    std::vector<VideoCaptureConfig> video_configs;
  };

  // Specific message type handler methods.
  void OnAnswer(ReceiverMessage message);

  // Used by SpawnSenders to generate a sender for a specific stream.
  std::unique_ptr<Sender> CreateSender(Ssrc receiver_ssrc,
                                       const Stream& stream,
                                       RtpPayloadType type);

  // Helper methods for spawning specific senders from the Answer message.
  void SpawnAudioSender(ConfiguredSenders* senders,
                        Ssrc receiver_ssrc,
                        int send_index,
                        int config_index);
  void SpawnVideoSender(ConfiguredSenders* senders,
                        Ssrc receiver_ssrc,
                        int send_index,
                        int config_index);

  // Spawn a set of configured senders from the currently stored negotiation.
  ConfiguredSenders SpawnSenders(const Answer& answer);

  // The remote address of the receiver we are communicating with. Used
  // for both TLS and UDP traffic.
  const IPAddress remote_address_;

  // The embedder is expected to provide us a client for notifications about
  // negotiations and errors, a valid cast environment, and a messaging
  // port for communicating to the Receiver over TLS.
  Client* const client_;
  Environment* const environment_;
  SenderSessionMessager messager_;

  // The packet router used for messaging across all senders.
  SenderPacketRouter packet_router_;

  // Each negotiation has its own sequence number, and the receiver replies
  // with the same sequence number that we send. Each message to the receiver
  // advances our current sequence number.
  int current_sequence_number_ = 0;

  // The current negotiation. If present, we are expected an ANSWER from
  // the receiver. If not present, any provided ANSWERS are rejected.
  std::unique_ptr<Negotiation> current_negotiation_;

  // If the negotiation has succeeded, we store the current audio and video
  // senders used for this session. Either or both may be nullptr.
  std::unique_ptr<Sender> current_audio_sender_;
  std::unique_ptr<Sender> current_video_sender_;
};  // namespace cast

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SENDER_SESSION_H_
