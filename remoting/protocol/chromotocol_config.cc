// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromotocol_config.h"

#include <algorithm>

namespace remoting {

const int kDefaultStreamVersion = 1;

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

ChromotocolConfig::ChromotocolConfig() { }

ChromotocolConfig::ChromotocolConfig(const ChromotocolConfig& config)
    : control_config_(config.control_config_),
      event_config_(config.event_config_),
      video_config_(config.video_config_),
      initial_resolution_(config.initial_resolution_) {
}

ChromotocolConfig::~ChromotocolConfig() { }

void ChromotocolConfig::SetControlConfig(const ChannelConfig& control_config) {
  control_config_ = control_config;
}
void ChromotocolConfig::SetEventConfig(const ChannelConfig& event_config) {
  event_config_ = event_config;
}
void ChromotocolConfig::SetVideoConfig(const ChannelConfig& video_config) {
  video_config_ = video_config;
}
void ChromotocolConfig::SetInitialResolution(const ScreenResolution& resolution) {
  initial_resolution_ = resolution;
}

ChromotocolConfig* ChromotocolConfig::Clone() const {
  return new ChromotocolConfig(*this);
}

// static
ChromotocolConfig* ChromotocolConfig::CreateDefault() {
  ChromotocolConfig* result = new ChromotocolConfig();
  result->SetControlConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                         kDefaultStreamVersion,
                                         ChannelConfig::CODEC_UNDEFINED));
  result->SetEventConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                        kDefaultStreamVersion,
                                        ChannelConfig::CODEC_UNDEFINED));
  result->SetVideoConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_ZIP));
  return result;
}

CandidateChromotocolConfig::CandidateChromotocolConfig() { }

CandidateChromotocolConfig::CandidateChromotocolConfig(
    const CandidateChromotocolConfig& config)
    : control_configs_(config.control_configs_),
      event_configs_(config.event_configs_),
      video_configs_(config.video_configs_),
      initial_resolution_(config.initial_resolution_) {
}

CandidateChromotocolConfig::~CandidateChromotocolConfig() { }

void CandidateChromotocolConfig::AddControlConfig(
    const ChannelConfig& control_config) {
  control_configs_.push_back(control_config);
}

void CandidateChromotocolConfig::AddEventConfig(
    const ChannelConfig& event_config) {
  event_configs_.push_back(event_config);
}

void CandidateChromotocolConfig::AddVideoConfig(
    const ChannelConfig& video_config) {
  video_configs_.push_back(video_config);
}

void CandidateChromotocolConfig::SetInitialResolution(
    const ScreenResolution& resolution) {
  initial_resolution_ = resolution;
}

ChromotocolConfig* CandidateChromotocolConfig::Select(
    const CandidateChromotocolConfig* client_config,
    bool force_host_resolution) {
  ChannelConfig control_config;
  ChannelConfig event_config;
  ChannelConfig video_config;
  if (!SelectCommonChannelConfig(
          control_configs_, client_config->control_configs_, &control_config) ||
      !SelectCommonChannelConfig(
          event_configs_, client_config->event_configs_, &event_config) ||
      !SelectCommonChannelConfig(
          video_configs_, client_config->video_configs_, &video_config)) {
    return NULL;
  }

  ChromotocolConfig* result = ChromotocolConfig::CreateDefault();
  result->SetControlConfig(control_config);
  result->SetEventConfig(event_config);
  result->SetVideoConfig(video_config);

  if (force_host_resolution) {
    result->SetInitialResolution(initial_resolution());
  } else {
    result->SetInitialResolution(client_config->initial_resolution());
  }

  return result;
}

bool CandidateChromotocolConfig::IsSupported(
    const ChromotocolConfig* config) const {
  return
      IsChannelConfigSupported(control_configs_, config->control_config()) &&
      IsChannelConfigSupported(event_configs_, config->event_config()) &&
      IsChannelConfigSupported(video_configs_, config->video_config()) &&
      config->initial_resolution().IsValid();
}

ChromotocolConfig* CandidateChromotocolConfig::GetFinalConfig() const {
  if (control_configs_.size() != 1 ||
      event_configs_.size() != 1 ||
      video_configs_.size() != 1) {
    return NULL;
  }

  ChromotocolConfig* result = ChromotocolConfig::CreateDefault();
  result->SetControlConfig(control_configs_.front());
  result->SetEventConfig(event_configs_.front());
  result->SetVideoConfig(video_configs_.front());
  result->SetInitialResolution(initial_resolution_);
  return result;
}

// static
bool CandidateChromotocolConfig::SelectCommonChannelConfig(
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
bool CandidateChromotocolConfig::IsChannelConfigSupported(
    const std::vector<ChannelConfig>& vector,
    const ChannelConfig& value) {
  return std::find(vector.begin(), vector.end(), value) != vector.end();
}

CandidateChromotocolConfig* CandidateChromotocolConfig::Clone() const {
  return new CandidateChromotocolConfig(*this);
}

// static
CandidateChromotocolConfig* CandidateChromotocolConfig::CreateEmpty() {
  return new CandidateChromotocolConfig();
}

// static
CandidateChromotocolConfig* CandidateChromotocolConfig::CreateFrom(
    const ChromotocolConfig* config) {
  CandidateChromotocolConfig* result = CreateEmpty();
  result->AddControlConfig(config->control_config());
  result->AddEventConfig(config->event_config());
  result->AddVideoConfig(config->video_config());
  result->SetInitialResolution(config->initial_resolution());
  return result;
}

// static
CandidateChromotocolConfig* CandidateChromotocolConfig::CreateDefault() {
  CandidateChromotocolConfig* result = CreateEmpty();
  result->AddControlConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                         kDefaultStreamVersion,
                                         ChannelConfig::CODEC_UNDEFINED));
  result->AddEventConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                          kDefaultStreamVersion,
                                          ChannelConfig::CODEC_UNDEFINED));

  result->AddVideoConfig(ChannelConfig(ChannelConfig::TRANSPORT_STREAM,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_ZIP));
  result->AddVideoConfig(ChannelConfig(ChannelConfig::TRANSPORT_SRTP,
                                       kDefaultStreamVersion,
                                       ChannelConfig::CODEC_VP8));
  return result;
}

}  // namespace remoting
