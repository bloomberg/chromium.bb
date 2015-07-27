// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_config.h"

#include <algorithm>

namespace remoting {
namespace protocol {

namespace {

bool IsChannelConfigSupported(const std::list<ChannelConfig>& list,
                              const ChannelConfig& value) {
  return std::find(list.begin(), list.end(), value) != list.end();
}

bool SelectCommonChannelConfig(const std::list<ChannelConfig>& host_configs,
                               const std::list<ChannelConfig>& client_configs,
                               ChannelConfig* config) {
  // Usually each of these lists will contain just a few elements, so iterating
  // over all of them is not a problem.
  std::list<ChannelConfig>::const_iterator it;
  for (it = client_configs.begin(); it != client_configs.end(); ++it) {
    if (IsChannelConfigSupported(host_configs, *it)) {
      *config = *it;
      return true;
    }
  }
  return false;
}

}  // namespace

const int kDefaultStreamVersion = 2;
const int kControlStreamVersion = 3;

ChannelConfig ChannelConfig::None() {
  return ChannelConfig();
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

// static
scoped_ptr<SessionConfig> SessionConfig::SelectCommon(
    const CandidateSessionConfig* client_config,
    const CandidateSessionConfig* host_config) {
  scoped_ptr<SessionConfig> result(new SessionConfig());
  ChannelConfig control_config;
  ChannelConfig event_config;
  ChannelConfig video_config;
  ChannelConfig audio_config;

  result->standard_ice_ =
      host_config->standard_ice() && client_config->standard_ice();

  if (!SelectCommonChannelConfig(host_config->control_configs(),
                                 client_config->control_configs(),
                                 &result->control_config_) ||
      !SelectCommonChannelConfig(host_config->event_configs(),
                                 client_config->event_configs(),
                                 &result->event_config_) ||
      !SelectCommonChannelConfig(host_config->video_configs(),
                                 client_config->video_configs(),
                                 &result->video_config_) ||
      !SelectCommonChannelConfig(host_config->audio_configs(),
                                 client_config->audio_configs(),
                                 &result->audio_config_)) {
    return nullptr;
  }

  return result;
}

// static
scoped_ptr<SessionConfig> SessionConfig::GetFinalConfig(
    const CandidateSessionConfig* candidate_config) {
  if (candidate_config->control_configs().size() != 1 ||
      candidate_config->event_configs().size() != 1 ||
      candidate_config->video_configs().size() != 1 ||
      candidate_config->audio_configs().size() != 1) {
    return nullptr;
  }

  scoped_ptr<SessionConfig> result(new SessionConfig());
  result->standard_ice_ = candidate_config->standard_ice();
  result->control_config_ = candidate_config->control_configs().front();
  result->event_config_ = candidate_config->event_configs().front();
  result->video_config_ = candidate_config->video_configs().front();
  result->audio_config_ = candidate_config->audio_configs().front();

  return result.Pass();
}

// static
scoped_ptr<SessionConfig> SessionConfig::ForTest() {
  scoped_ptr<SessionConfig> result(new SessionConfig());
  result->standard_ice_ = true;
  result->control_config_ = ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                                          kControlStreamVersion,
                                          ChannelConfig::CODEC_UNDEFINED);
  result->event_config_ = ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_UNDEFINED);
  result->video_config_ = ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_VP8);
  result->audio_config_ = ChannelConfig(ChannelConfig::TRANSPORT_NONE,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_UNDEFINED);
  return result.Pass();
}

SessionConfig::SessionConfig() {
}

CandidateSessionConfig::CandidateSessionConfig() { }

CandidateSessionConfig::CandidateSessionConfig(
    const CandidateSessionConfig& config) = default;

CandidateSessionConfig::~CandidateSessionConfig() { }

bool CandidateSessionConfig::IsSupported(
    const SessionConfig& config) const {
  return
      IsChannelConfigSupported(control_configs_, config.control_config()) &&
      IsChannelConfigSupported(event_configs_, config.event_config()) &&
      IsChannelConfigSupported(video_configs_, config.video_config()) &&
      IsChannelConfigSupported(audio_configs_, config.audio_config());
}

scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::Clone() const {
  return make_scoped_ptr(new CandidateSessionConfig(*this));
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateEmpty() {
  return make_scoped_ptr(new CandidateSessionConfig());
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateFrom(
    const SessionConfig& config) {
  scoped_ptr<CandidateSessionConfig> result = CreateEmpty();
  result->set_standard_ice(config.standard_ice());
  result->mutable_control_configs()->push_back(config.control_config());
  result->mutable_event_configs()->push_back(config.event_config());
  result->mutable_video_configs()->push_back(config.video_config());
  result->mutable_audio_configs()->push_back(config.audio_config());
  return result.Pass();
}

// static
scoped_ptr<CandidateSessionConfig> CandidateSessionConfig::CreateDefault() {
  scoped_ptr<CandidateSessionConfig> result = CreateEmpty();

  result->set_standard_ice(true);

  // Control channel.
  result->mutable_control_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kControlStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Event channel.
  result->mutable_event_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));

  // Video channel.
  result->mutable_video_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP8));

  // Audio channel.
  result->mutable_audio_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_MUX_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_OPUS));
  result->mutable_audio_configs()->push_back(ChannelConfig::None());

  return result.Pass();
}

void CandidateSessionConfig::DisableAudioChannel() {
  mutable_audio_configs()->clear();
  mutable_audio_configs()->push_back(ChannelConfig());
}

void CandidateSessionConfig::EnableVideoCodec(ChannelConfig::Codec codec) {
  mutable_video_configs()->push_front(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    codec));
}

}  // namespace protocol
}  // namespace remoting
