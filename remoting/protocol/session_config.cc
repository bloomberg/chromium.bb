// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_config.h"

#include <algorithm>

namespace remoting {
namespace protocol {

const int kDefaultStreamVersion = 2;

namespace {
const int kDefaultWidth = 800;
const int kDefaultHeight = 600;
}  // namespace

ChannelConfig::ChannelConfig() {
  Reset();
}

ChannelConfig::ChannelConfig(TransportType transport, int version, Codec codec)
    : transport(transport),
      version(version),
      codec(codec) {
}

bool ChannelConfig::operator==(const ChannelConfig& b) const {
  return transport == b.transport && version == b.version && codec == b.codec;
}

void ChannelConfig::Reset() {
  transport = TRANSPORT_STREAM;
  version = kDefaultStreamVersion;
  codec = CODEC_UNDEFINED;
}

ScreenResolution::ScreenResolution()
    : width(kDefaultWidth),
      height(kDefaultHeight) {
}

ScreenResolution::ScreenResolution(int width, int height)
    : width(width),
      height(height) {
}

bool ScreenResolution::IsValid() const {
  return width > 0 && height > 0;
}

SessionConfig::SessionConfig() { }

SessionConfig::~SessionConfig() { }

void SessionConfig::SetControlConfig(const ChannelConfig& control_config) {
  control_config_ = control_config;
}
void SessionConfig::SetEventConfig(const ChannelConfig& event_config) {
  event_config_ = event_config;
}
void SessionConfig::SetVideoConfig(const ChannelConfig& video_config) {
  video_config_ = video_config;
}
void SessionConfig::SetInitialResolution(const ScreenResolution& resolution) {
  initial_resolution_ = resolution;
}

// static
SessionConfig SessionConfig::GetDefault() {
  SessionConfig result;
  result.SetControlConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                         kDefaultStreamVersion,
                                         ChannelConfig::CODEC_UNDEFINED));
  result.SetEventConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_UNDEFINED));
  result.SetVideoConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_VP8));
  return result;
}

CandidateSessionConfig::CandidateSessionConfig() { }

CandidateSessionConfig::CandidateSessionConfig(
    const CandidateSessionConfig& config)
    : control_configs_(config.control_configs_),
      event_configs_(config.event_configs_),
      video_configs_(config.video_configs_),
      initial_resolution_(config.initial_resolution_) {
}

CandidateSessionConfig::~CandidateSessionConfig() { }

bool CandidateSessionConfig::Select(
    const CandidateSessionConfig* client_config,
    bool force_host_resolution,
    SessionConfig* result) {
  ChannelConfig control_config;
  ChannelConfig event_config;
  ChannelConfig video_config;

  if (!SelectCommonChannelConfig(
          control_configs_, client_config->control_configs_, &control_config) ||
      !SelectCommonChannelConfig(
          event_configs_, client_config->event_configs_, &event_config) ||
      !SelectCommonChannelConfig(
          video_configs_, client_config->video_configs_, &video_config)) {
    return false;
  }

  result->SetControlConfig(control_config);
  result->SetEventConfig(event_config);
  result->SetVideoConfig(video_config);

  if (force_host_resolution) {
    result->SetInitialResolution(initial_resolution());
  } else {
    result->SetInitialResolution(client_config->initial_resolution());
  }

  return true;
}

bool CandidateSessionConfig::IsSupported(
    const SessionConfig& config) const {
  return
      IsChannelConfigSupported(control_configs_, config.control_config()) &&
      IsChannelConfigSupported(event_configs_, config.event_config()) &&
      IsChannelConfigSupported(video_configs_, config.video_config()) &&
      config.initial_resolution().IsValid();
}

bool CandidateSessionConfig::GetFinalConfig(SessionConfig* result) const {
  if (control_configs_.size() != 1 ||
      event_configs_.size() != 1 ||
      video_configs_.size() != 1) {
    return false;
  }

  result->SetControlConfig(control_configs_.front());
  result->SetEventConfig(event_configs_.front());
  result->SetVideoConfig(video_configs_.front());
  result->SetInitialResolution(initial_resolution_);

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

CandidateSessionConfig* CandidateSessionConfig::Clone() const {
  return new CandidateSessionConfig(*this);
}

// static
CandidateSessionConfig* CandidateSessionConfig::CreateEmpty() {
  return new CandidateSessionConfig();
}

// static
CandidateSessionConfig* CandidateSessionConfig::CreateFrom(
    const SessionConfig& config) {
  CandidateSessionConfig* result = CreateEmpty();
  result->mutable_control_configs()->push_back(config.control_config());
  result->mutable_event_configs()->push_back(config.event_config());
  result->mutable_video_configs()->push_back(config.video_config());
  *result->mutable_initial_resolution() = config.initial_resolution();
  return result;
}

// static
CandidateSessionConfig* CandidateSessionConfig::CreateDefault() {
  CandidateSessionConfig* result = CreateEmpty();
  result->mutable_control_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));
  result->mutable_event_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_UNDEFINED));
  result->mutable_video_configs()->push_back(
      ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                    kDefaultStreamVersion,
                    ChannelConfig::CODEC_VP8));
  return result;
}

}  // namespace protocol
}  // namespace remoting
