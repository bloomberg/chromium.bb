/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rate_control_settings.h"

#include <inttypes.h>
#include <stdio.h>

#include <string>

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace {

const int kDefaultAcceptedQueueMs = 250;

const int kDefaultMinPushbackTargetBitrateBps = 30000;

const char kVp8TrustedRateControllerFieldTrialName[] =
    "WebRTC-LibvpxVp8TrustedRateController";
const char kVp9TrustedRateControllerFieldTrialName[] =
    "WebRTC-LibvpxVp9TrustedRateController";

const char* kVideoHysteresisFieldTrialname =
    "WebRTC-SimulcastUpswitchHysteresisPercent";
const double kDefaultVideoHysteresisFactor = 1.0;
const char* kScreenshareHysteresisFieldTrialname =
    "WebRTC-SimulcastScreenshareUpswitchHysteresisPercent";
// Default to 35% hysteresis for simulcast screenshare.
const double kDefaultScreenshareHysteresisFactor = 1.35;

bool IsEnabled(const WebRtcKeyValueConfig* const key_value_config,
               absl::string_view key) {
  return key_value_config->Lookup(key).find("Enabled") == 0;
}

double ParseHysteresisFactor(const WebRtcKeyValueConfig* const key_value_config,
                             absl::string_view key,
                             double default_value) {
  std::string group_name = key_value_config->Lookup(key);
  int percent = 0;
  if (!group_name.empty() && sscanf(group_name.c_str(), "%d", &percent) == 1 &&
      percent >= 0) {
    return 1.0 + (percent / 100.0);
  }
  return default_value;
}

}  // namespace

RateControlSettings::RateControlSettings(
    const WebRtcKeyValueConfig* const key_value_config)
    : congestion_window_("QueueSize"),
      congestion_window_pushback_("MinBitrate"),
      pacing_factor_("pacing_factor"),
      alr_probing_("alr_probing", false),
      vp8_qp_max_("vp8_qp_max"),
      trust_vp8_(
          "trust_vp8",
          IsEnabled(key_value_config, kVp8TrustedRateControllerFieldTrialName)),
      trust_vp9_(
          "trust_vp9",
          IsEnabled(key_value_config, kVp9TrustedRateControllerFieldTrialName)),
      video_hysteresis_("video_hysteresis",
                        ParseHysteresisFactor(key_value_config,
                                              kVideoHysteresisFieldTrialname,
                                              kDefaultVideoHysteresisFactor)),
      screenshare_hysteresis_(
          "screenshare_hysteresis",
          ParseHysteresisFactor(key_value_config,
                                kScreenshareHysteresisFieldTrialname,
                                kDefaultScreenshareHysteresisFactor)),
      probe_max_allocation_("probe_max_allocation", true),
      bitrate_adjuster_("bitrate_adjuster", false),
      adjuster_use_headroom_("adjuster_use_headroom", false),
      vp8_s0_boost_("vp8_s0_boost", true),
      vp8_dynamic_rate_("vp8_dynamic_rate", false),
      vp9_dynamic_rate_("vp9_dynamic_rate", false) {
  ParseFieldTrial({&congestion_window_, &congestion_window_pushback_},
                  key_value_config->Lookup("WebRTC-CongestionWindow"));
  ParseFieldTrial(
      {&pacing_factor_, &alr_probing_, &vp8_qp_max_, &trust_vp8_, &trust_vp9_,
       &video_hysteresis_, &screenshare_hysteresis_, &probe_max_allocation_,
       &bitrate_adjuster_, &adjuster_use_headroom_, &vp8_s0_boost_,
       &vp8_dynamic_rate_, &vp9_dynamic_rate_},
      key_value_config->Lookup("WebRTC-VideoRateControl"));
}

RateControlSettings::~RateControlSettings() = default;
RateControlSettings::RateControlSettings(RateControlSettings&&) = default;

RateControlSettings RateControlSettings::ParseFromFieldTrials() {
  FieldTrialBasedConfig field_trial_config;
  return RateControlSettings(&field_trial_config);
}

RateControlSettings RateControlSettings::ParseFromKeyValueConfig(
    const WebRtcKeyValueConfig* const key_value_config) {
  FieldTrialBasedConfig field_trial_config;
  return RateControlSettings(key_value_config ? key_value_config
                                              : &field_trial_config);
}

bool RateControlSettings::UseCongestionWindow() const {
  return static_cast<bool>(congestion_window_);
}

int64_t RateControlSettings::GetCongestionWindowAdditionalTimeMs() const {
  return congestion_window_.GetOptional().value_or(kDefaultAcceptedQueueMs);
}

bool RateControlSettings::UseCongestionWindowPushback() const {
  return congestion_window_ && congestion_window_pushback_;
}

uint32_t RateControlSettings::CongestionWindowMinPushbackTargetBitrateBps()
    const {
  return congestion_window_pushback_.GetOptional().value_or(
      kDefaultMinPushbackTargetBitrateBps);
}

absl::optional<double> RateControlSettings::GetPacingFactor() const {
  return pacing_factor_.GetOptional();
}

bool RateControlSettings::UseAlrProbing() const {
  return alr_probing_.Get();
}

absl::optional<int> RateControlSettings::LibvpxVp8QpMax() const {
  if (vp8_qp_max_ && (vp8_qp_max_.Value() < 0 || vp8_qp_max_.Value() > 63)) {
    RTC_LOG(LS_WARNING) << "Unsupported vp8_qp_max_ value, ignored.";
    return absl::nullopt;
  }
  return vp8_qp_max_.GetOptional();
}

bool RateControlSettings::LibvpxVp8TrustedRateController() const {
  return trust_vp8_.Get();
}

bool RateControlSettings::Vp8BoostBaseLayerQuality() const {
  return vp8_s0_boost_.Get();
}

bool RateControlSettings::Vp8DynamicRateSettings() const {
  return vp8_dynamic_rate_.Get();
}

bool RateControlSettings::LibvpxVp9TrustedRateController() const {
  return trust_vp9_.Get();
}

bool RateControlSettings::Vp9DynamicRateSettings() const {
  return vp9_dynamic_rate_.Get();
}

double RateControlSettings::GetSimulcastHysteresisFactor(
    VideoCodecMode mode) const {
  if (mode == VideoCodecMode::kScreensharing) {
    return GetSimulcastScreenshareHysteresisFactor();
  }
  return GetSimulcastVideoHysteresisFactor();
}

double RateControlSettings::GetSimulcastHysteresisFactor(
    VideoEncoderConfig::ContentType content_type) const {
  if (content_type == VideoEncoderConfig::ContentType::kScreen) {
    return GetSimulcastScreenshareHysteresisFactor();
  }
  return GetSimulcastVideoHysteresisFactor();
}

double RateControlSettings::GetSimulcastVideoHysteresisFactor() const {
  return video_hysteresis_.Get();
}

double RateControlSettings::GetSimulcastScreenshareHysteresisFactor() const {
  return screenshare_hysteresis_.Get();
}

bool RateControlSettings::TriggerProbeOnMaxAllocatedBitrateChange() const {
  return probe_max_allocation_.Get();
}

bool RateControlSettings::UseEncoderBitrateAdjuster() const {
  return bitrate_adjuster_.Get();
}

bool RateControlSettings::BitrateAdjusterCanUseNetworkHeadroom() const {
  return adjuster_use_headroom_.Get();
}

}  // namespace webrtc
