// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_config.h"

#include <algorithm>

namespace remoting {
namespace protocol {

const int kDefaultStreamVersion = 2;

ChannelConfig::ChannelConfig()
    : transport(TRANSPORT_NONE),
      version(0),
      codec(CODEC_UNDEFINED) {
}

ChannelConfig::ChannelConfig(TransportType transport, int version, Codec codec)
    : transport(transport),
      version(version),
      codec(codec) {
}

bool ChannelConfig::operator==(const ChannelConfig& b) const {
  // If the transport field is set to NONE then all other fields are irrelevant.
  if (transport == ChannelConfig::TRANSPORT_NONE)
    return transport == b.transport;
  return transport == b.transport && version == b.version && codec == b.codec;
}

SessionConfig::SessionConfig() {
}

// static
SessionConfig SessionConfig::GetDefault() {
  SessionConfig result;
  result.set_control_config(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                          kDefaultStreamVersion,
                                          ChannelConfig::CODEC_UNDEFINED));
  result.set_event_config(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_UNDEFINED));
  result.set_video_config(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_VP8));
  result.set_audio_config(ChannelConfig(ChannelConfig::TRANSPORT_NONE,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_VERBATIM));
  return result;
}

CandidateSessionConfig::CandidateSessionConfig() { }

CandidateSessionConfig::CandidateSessionConfig(
    const CandidateSessionConfig& config)
    : control_configs_(config.control_configs_),
      event_configs_(config.event_configs_),
      video_configs_(config.video_configs_),
      audio_configs_(config.audio_configs_) {
}

CandidateSessionConfig::~CandidateSessionConfig() { }

bool CandidateSessionConfig::Select(
    const CandidateSessionConfig* client_config,
    SessionConfig* result) {
  ChannelConfig control_config;
  ChannelConfig event_config;
  ChannelConfig video_config;
  ChannelConfig audio_config;

  if (!SelectCommonChannelConfig(
          control_configs_, client_config->control_configs_, &control_config) ||
      !SelectCommonChannelConfig(
          event_configs_, client_config->event_configs_, &event_config) ||
      !SelectCommonChannelConfig(
          video_configs_, client_config->video_configs_, &video_config) ||
      !SelectCommonChannelConfig(
          audio_configs_, client_config->audio_configs_, &audio_config)) {
    return false;
  }

  result->set_control_config(control_config);
  result->set_event_config(event_config);
  result->set_video_config(video_config);
  result->set_audio_config(audio_config);

  return true;
}

bool CandidateSessionConfig::IsSupported(
    const SessionConfig& config) const {
  return
      IsChannelConfigSupported(control_configs_, config.control_config()) &&
      IsChannelConfigSupported(event_configs_, config.event_config()) &&
      IsChannelConfigSupported(video_configs_, config.video_config()) &&
      IsChannelConfigSupported(audio_configs_, config.audio_config());
}

bool CandidateSessionConfig::GetFinalConfig(SessionConfig* result) const {
  if (control_configs_.size() != 1 ||
      event_configs_.size() != 1 ||
      video_configs_.size() != 1 ||
      audio_configs_.size() != 1) {
    return false;
  }

  result->set_control_config(control_configs_.front());
  result->set_event_config(event_configs_.front());
  result->set_video_config(video_configs_.front());
  result->set_audio_config(audio_configs_.front());

  return true;
}

// static
bool CandidateSessionConfig::SelectCommonChannelConfig(
    const std::vector<ChannelConfig>& host_configs,
    const std::vector<ChannelConfig>& client_configs,
    ChannelConfig* config) {
  // Usually each of these vectors will contain just several elements,
  // so iterating over all of them is not a problem.
  std::vector<ChannelConfig>::const_iterator it;
  for (it = client_configs.begin(); it != client_configs.end(); ++it) {
    if (IsChannelConfigSupported(host_configs, *it)) {
      *config = *it;
      return true;
    }
  }
  return false;
}

// static
bool CandidateSessionConfig::IsChannelConfigSupported(
    const std::vector<ChannelConfig>& vector,
    const ChannelConfig& value) {
  return std::find(vector.begin(), vector.end(), value) != vector.end();
}

scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::Clone() const {
  return scoped_ptr<CandidateSessionConfig>(new CandidateSessionConfig(*this));
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateEmpty() {
  return scoped_ptr<CandidateSessionConfig>(new CandidateSessionConfig());
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateFrom(
    const SessionConfig& config) {
  scoped_ptr<CandidateSessionConfig> result = CreateEmpty();
  result->mutable_control_configs()->push_back(config.control_config());
  result->mutable_event_configs()->push_back(config.event_config());
  result->mutable_video_configs()->push_back(config.video_config());
  result->mutable_audio_configs()->push_back(config.audio_config());
  return result.Pass();
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateDefault() {
  scoped_ptr<CandidateSessionConfig> result = CreateEmpty();

  // Control channel.
  result->mutable_control_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));
  result->mutable_control_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Event channel.
  result->mutable_event_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));
  result->mutable_event_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Video channel.
  result->mutable_video_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP8));

  // Audio channel.
  result->mutable_audio_configs()->push_back(ChannelConfig());
#if defined(ENABLE_REMOTING_AUDIO)
  EnableAudioChannel(result.get());
#endif  // defined(ENABLE_REMOTING_AUDIO)

  return result.Pass();
}

// static
void CandidateSessionConfig::EnableAudioChannel(
  CandidateSessionConfig* config) {
  config->mutable_audio_configs()->clear();
  config->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_SPEEX));
  config->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_SPEEX));
  config->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_VERBATIM));
  config->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_VERBATIM));
  config->mutable_audio_configs()->push_back(ChannelConfig());
}

}  // namespace protocol
}  // namespace remoting
