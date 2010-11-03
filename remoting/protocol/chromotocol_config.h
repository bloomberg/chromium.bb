// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMOTOCOL_CONFIG_H_
#define REMOTING_PROTOCOL_CHROMOTOCOL_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace remoting {

extern const int kDefaultStreamVersion;

// Struct for configuration parameters of a single channel.
struct ChannelConfig {
  enum TransportType {
    TRANSPORT_STREAM,
    TRANSPORT_DATAGRAM,
    TRANSPORT_SRTP,
    TRANSPORT_RTP_DTLS,
  };

  enum Codec {
    CODEC_UNDEFINED,  // Used for event and control channels.
    CODEC_VERBATIM,
    CODEC_ZIP,
    CODEC_VP8,
  };

  ChannelConfig();
  ChannelConfig(TransportType transport, int version, Codec codec);

  // operator== is overloaded so that std::find() works with
  // std::vector<ChannelConfig>.
  bool operator==(const ChannelConfig& b) const;

  void Reset();

  TransportType transport;
  int version;
  Codec codec;
};

struct ScreenResolution {
  ScreenResolution();
  ScreenResolution(int width, int height);

  bool IsValid() const;

  int width;
  int height;
};

// ChromotocolConfig is used by ChromotingConnection to store negotiated
// chromotocol configuration.
class ChromotocolConfig {
 public:
  ~ChromotocolConfig();

  const ChannelConfig& control_config() const { return control_config_; }
  const ChannelConfig& event_config() const { return event_config_; }
  const ChannelConfig& video_config() const { return video_config_; }
  const ScreenResolution& initial_resolution() const {
    return initial_resolution_;
  }

  void SetControlConfig(const ChannelConfig& control_config);
  void SetEventConfig(const ChannelConfig& event_config);
  void SetVideoConfig(const ChannelConfig& video_config);
  void SetInitialResolution(const ScreenResolution& initial_resolution);

  ChromotocolConfig* Clone() const;

  static ChromotocolConfig* CreateDefault();

 private:
  ChromotocolConfig();
  explicit ChromotocolConfig(const ChromotocolConfig& config);
  ChromotocolConfig& operator=(const ChromotocolConfig& b);

  ChannelConfig control_config_;
  ChannelConfig event_config_;
  ChannelConfig video_config_;
  ScreenResolution initial_resolution_;
};

// Defines session description that is sent from client to the host in the
// session-initiate message. It is different from the regular ChromotocolConfig
// because it allows one to specify multiple configurations for each channel.
class CandidateChromotocolConfig {
 public:
  ~CandidateChromotocolConfig();

  const std::vector<ChannelConfig>& control_configs() const {
    return control_configs_;
  }

  const std::vector<ChannelConfig>& event_configs() const {
    return event_configs_;
  }

  const std::vector<ChannelConfig>& video_configs() const {
    return video_configs_;
  }

  const ScreenResolution& initial_resolution() const {
    return initial_resolution_;
  }

  void AddControlConfig(const ChannelConfig& control_config);
  void AddEventConfig(const ChannelConfig& event_config);
  void AddVideoConfig(const ChannelConfig& video_config);
  void SetInitialResolution(const ScreenResolution& initial_resolution);

  // Selects session configuration that is supported by both participants.
  // NULL is returned if such configuration doesn't exist. When selecting
  // channel configuration priority is given to the configs listed first
  // in |client_config|.
  ChromotocolConfig* Select(const CandidateChromotocolConfig* client_config,
                         bool force_host_resolution);

  // Returns true if |config| is supported.
  bool IsSupported(const ChromotocolConfig* config) const;

  // Extracts final protocol configuration. Must be used for the description
  // received in the session-accept stanza. If the selection is ambiguous
  // (e.g. there is more than one configuration for one of the channel)
  // or undefined (e.g. no configurations for a channel) then NULL is returned.
  ChromotocolConfig* GetFinalConfig() const;

  CandidateChromotocolConfig* Clone() const;

  static CandidateChromotocolConfig* CreateEmpty();
  static CandidateChromotocolConfig* CreateFrom(
      const ChromotocolConfig* config);
  static CandidateChromotocolConfig* CreateDefault();

 private:
  CandidateChromotocolConfig();
  explicit CandidateChromotocolConfig(const CandidateChromotocolConfig& config);
  CandidateChromotocolConfig& operator=(const CandidateChromotocolConfig& b);

  static bool SelectCommonChannelConfig(
      const std::vector<ChannelConfig>& host_configs_,
      const std::vector<ChannelConfig>& client_configs_,
      ChannelConfig* config);
  static bool IsChannelConfigSupported(const std::vector<ChannelConfig>& vector,
                                       const ChannelConfig& value);

  std::vector<ChannelConfig> control_configs_;
  std::vector<ChannelConfig> event_configs_;
  std::vector<ChannelConfig> video_configs_;

  ScreenResolution initial_resolution_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMOTOCOL_CONFIG_H_
