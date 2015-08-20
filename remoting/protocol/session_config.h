// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_CONFIG_H_
#define REMOTING_PROTOCOL_SESSION_CONFIG_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {
namespace protocol {

extern const int kDefaultStreamVersion;

// Struct for configuration parameters of a single channel.
// Some channels (like video) may have multiple underlying sockets that need
// to be configured simultaneously.
struct ChannelConfig {
  enum TransportType {
    TRANSPORT_STREAM,
    TRANSPORT_MUX_STREAM,
    TRANSPORT_QUIC_STREAM,
    TRANSPORT_DATAGRAM,
    TRANSPORT_NONE,
  };

  enum Codec {
    CODEC_UNDEFINED,  // Used for event and control channels.
    CODEC_VERBATIM,
    CODEC_ZIP,
    CODEC_VP8,
    CODEC_VP9,
    CODEC_OPUS,
    CODEC_SPEEX,
  };

  // Creates a config with transport field set to TRANSPORT_NONE which indicates
  // that corresponding channel is disabled.
  static ChannelConfig None();

  // Default constructor. Equivalent to None().
  ChannelConfig() = default;

  // Creates a channel config with the specified parameters.
  ChannelConfig(TransportType transport, int version, Codec codec);

  // operator== is overloaded so that std::find() works with
  // std::list<ChannelConfig>.
  bool operator==(const ChannelConfig& b) const;

  TransportType transport = TRANSPORT_NONE;
  int version = 0;
  Codec codec = CODEC_UNDEFINED;
};

class CandidateSessionConfig;

// SessionConfig is used by the chromoting Session to store negotiated
// chromotocol configuration.
class SessionConfig {
 public:
  // Selects session configuration that is supported by both participants.
  // nullptr is returned if such configuration doesn't exist. When selecting
  // channel configuration priority is given to the configs listed first
  // in |client_config|.
  static scoped_ptr<SessionConfig> SelectCommon(
      const CandidateSessionConfig* client_config,
      const CandidateSessionConfig* host_config);

  // Extracts final protocol configuration. Must be used for the description
  // received in the session-accept stanza. If the selection is ambiguous
  // (e.g. there is more than one configuration for one of the channel)
  // or undefined (e.g. no configurations for a channel) then nullptr is
  // returned.
  static scoped_ptr<SessionConfig> GetFinalConfig(
      const CandidateSessionConfig* candidate_config);

  // Returns a suitable session configuration for use in tests.
  static scoped_ptr<SessionConfig> ForTest();
  static scoped_ptr<SessionConfig> ForTestWithVerbatimVideo();

  bool standard_ice() const { return standard_ice_; }

  const ChannelConfig& control_config() const { return control_config_; }
  const ChannelConfig& event_config() const { return event_config_; }
  const ChannelConfig& video_config() const { return video_config_; }
  const ChannelConfig& audio_config() const { return audio_config_; }

  bool is_audio_enabled() const {
    return audio_config_.transport != ChannelConfig::TRANSPORT_NONE;
  }

  // Returns true if any of the channels is using QUIC.
  bool is_using_quic() const {
    return control_config_.transport == ChannelConfig::TRANSPORT_QUIC_STREAM ||
           event_config_.transport == ChannelConfig::TRANSPORT_QUIC_STREAM ||
           video_config_.transport == ChannelConfig::TRANSPORT_QUIC_STREAM ||
           audio_config_.transport == ChannelConfig::TRANSPORT_QUIC_STREAM;
  }

 private:
  SessionConfig();

  bool standard_ice_ = true;

  ChannelConfig control_config_;
  ChannelConfig event_config_;
  ChannelConfig video_config_;
  ChannelConfig audio_config_;
};

// Defines session description that is sent from client to the host in the
// session-initiate message. It is different from the regular Config
// because it allows one to specify multiple configurations for each channel.
class CandidateSessionConfig {
 public:
  static scoped_ptr<CandidateSessionConfig> CreateEmpty();
  static scoped_ptr<CandidateSessionConfig> CreateFrom(
      const SessionConfig& config);
  static scoped_ptr<CandidateSessionConfig> CreateDefault();

  ~CandidateSessionConfig();

  bool standard_ice() const { return standard_ice_; }
  void set_standard_ice(bool standard_ice) { standard_ice_ = standard_ice; }

  const std::list<ChannelConfig>& control_configs() const {
    return control_configs_;
  }

  std::list<ChannelConfig>* mutable_control_configs() {
    return &control_configs_;
  }

  const std::list<ChannelConfig>& event_configs() const {
    return event_configs_;
  }

  std::list<ChannelConfig>* mutable_event_configs() {
    return &event_configs_;
  }

  const std::list<ChannelConfig>& video_configs() const {
    return video_configs_;
  }

  std::list<ChannelConfig>* mutable_video_configs() {
    return &video_configs_;
  }

  const std::list<ChannelConfig>& audio_configs() const {
    return audio_configs_;
  }

  std::list<ChannelConfig>* mutable_audio_configs() {
    return &audio_configs_;
  }

  // Old clients always list VP9 as supported and preferred even though they
  // shouldn't have it enabled yet. I.e. the host cannot trust VP9 codec
  // preference received from the client. To workaround this issue the client
  // adds a hint in the session-initiate message to indicate that it actually
  // wants VP9 to be enabled.
  //
  // TODO(sergeyu): Remove this kludge as soon as VP9 is enabled by default.
  bool vp9_experiment_enabled() const { return vp9_experiment_enabled_; }
  void set_vp9_experiment_enabled(bool value) {
    vp9_experiment_enabled_ = value;
  }

  // Returns true if |config| is supported.
  bool IsSupported(const SessionConfig& config) const;

  scoped_ptr<CandidateSessionConfig> Clone() const;

  // Helpers for enabling/disabling specific features.
  void DisableAudioChannel();
  void PreferTransport(ChannelConfig::TransportType transport);

 private:
  CandidateSessionConfig();
  explicit CandidateSessionConfig(const CandidateSessionConfig& config);
  CandidateSessionConfig& operator=(const CandidateSessionConfig& b);

  bool standard_ice_ = true;

  std::list<ChannelConfig> control_configs_;
  std::list<ChannelConfig> event_configs_;
  std::list<ChannelConfig> video_configs_;
  std::list<ChannelConfig> audio_configs_;

  bool vp9_experiment_enabled_ = false;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_CONFIG_H_
