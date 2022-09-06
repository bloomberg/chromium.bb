/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/peerconnection_quality_test_fixture.h"

#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using VideoCodecConfig = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoCodecConfig;
using VideoSubscription = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoSubscription;

std::string SpecToString(
    PeerConnectionE2EQualityTestFixture::VideoResolution::VideoResolution::Spec
        spec) {
  switch (spec) {
    case PeerConnectionE2EQualityTestFixture::VideoResolution::Spec::kNone:
      return "None";
    case PeerConnectionE2EQualityTestFixture::VideoResolution::Spec::
        kMaxFromSender:
      return "MaxFromSender";
  }
}

}  // namespace

PeerConnectionE2EQualityTestFixture::VideoResolution::VideoResolution(
    size_t width,
    size_t height,
    int32_t fps)
    : width_(width), height_(height), fps_(fps), spec_(Spec::kNone) {}
PeerConnectionE2EQualityTestFixture::VideoResolution::VideoResolution(Spec spec)
    : width_(0), height_(0), fps_(0), spec_(spec) {}

bool PeerConnectionE2EQualityTestFixture::VideoResolution::operator==(
    const VideoResolution& other) const {
  if (spec_ != Spec::kNone && spec_ == other.spec_) {
    // If there is some particular spec set, then it doesn't matter what
    // values we have in other fields.
    return true;
  }
  return width_ == other.width_ && height_ == other.height_ &&
         fps_ == other.fps_ && spec_ == other.spec_;
}

std::string PeerConnectionE2EQualityTestFixture::VideoResolution::ToString()
    const {
  rtc::StringBuilder out;
  out << "{ width=" << width_ << ", height=" << height_ << ", fps=" << fps_
      << ", spec=" << SpecToString(spec_) << " }";
  return out.Release();
}

bool PeerConnectionE2EQualityTestFixture::VideoSubscription::operator==(
    const VideoSubscription& other) const {
  return default_resolution_ == other.default_resolution_ &&
         peers_resolution_ == other.peers_resolution_;
}

absl::optional<PeerConnectionE2EQualityTestFixture::VideoResolution>
PeerConnectionE2EQualityTestFixture::VideoSubscription::GetMaxResolution(
    rtc::ArrayView<const VideoConfig> video_configs) {
  std::vector<VideoResolution> resolutions;
  for (const auto& video_config : video_configs) {
    resolutions.push_back(video_config.GetResolution());
  }
  return GetMaxResolution(resolutions);
}

absl::optional<PeerConnectionE2EQualityTestFixture::VideoResolution>
PeerConnectionE2EQualityTestFixture::VideoSubscription::GetMaxResolution(
    rtc::ArrayView<const VideoResolution> resolutions) {
  if (resolutions.empty()) {
    return absl::nullopt;
  }

  VideoResolution max_resolution;
  for (const VideoResolution& resolution : resolutions) {
    if (max_resolution.width() < resolution.width()) {
      max_resolution.set_width(resolution.width());
    }
    if (max_resolution.height() < resolution.height()) {
      max_resolution.set_height(resolution.height());
    }
    if (max_resolution.fps() < resolution.fps()) {
      max_resolution.set_fps(resolution.fps());
    }
  }
  return max_resolution;
}

std::string PeerConnectionE2EQualityTestFixture::VideoSubscription::ToString()
    const {
  rtc::StringBuilder out;
  out << "{ default_resolution_=[";
  if (default_resolution_.has_value()) {
    out << default_resolution_->ToString();
  } else {
    out << "undefined";
  }
  out << "], {";
  for (const auto& [peer_name, resolution] : peers_resolution_) {
    out << "[" << peer_name << ": " << resolution.ToString() << "], ";
  }
  out << "} }";
  return out.Release();
}

PeerConnectionE2EQualityTestFixture::VideoConfig::VideoConfig(
    const VideoResolution& resolution)
    : width(resolution.width()),
      height(resolution.height()),
      fps(resolution.fps()) {
  RTC_CHECK(resolution.IsRegular());
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
