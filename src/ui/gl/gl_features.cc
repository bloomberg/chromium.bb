// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#endif

namespace features {
namespace {

const base::Feature kGpuVsync{"GpuVsync", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByBrand{
        &kDefaultPassthroughCommandDecoder, "BlockListByBrand", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByDevice{
        &kDefaultPassthroughCommandDecoder, "BlockListByDevice", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByAndroidBuildId{
        &kDefaultPassthroughCommandDecoder, "BlockListByAndroidBuildId", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByManufacturer{
        &kDefaultPassthroughCommandDecoder, "BlockListByManufacturer", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByModel{
        &kDefaultPassthroughCommandDecoder, "BlockListByModel", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByBoard{
        &kDefaultPassthroughCommandDecoder, "BlockListByBoard", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByAndroidBuildFP{
        &kDefaultPassthroughCommandDecoder, "BlockListByAndroidBuildFP", ""};

bool IsDeviceBlocked(const char* field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}
#endif

}  // namespace

// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
// Launched on Windows, still experimental on other platforms.
const base::Feature kDefaultPassthroughCommandDecoder {
  "DefaultPassthroughCommandDecoder",
#if defined(OS_WIN) || defined(OS_FUCHSIA) ||                \
    ((defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
     !defined(CHROMECAST_BUILD))
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool UseGpuVsync() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         base::FeatureList::IsEnabled(kGpuVsync);
}

bool UsePassthroughCommandDecoder() {
  if (!base::FeatureList::IsEnabled(kDefaultPassthroughCommandDecoder))
    return false;

#if defined(OS_ANDROID)
  // Check block list against build info.
  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->brand(),
                      kPassthroughCommandDecoderBlockListByBrand.Get()))
    return false;
  if (IsDeviceBlocked(build_info->device(),
                      kPassthroughCommandDecoderBlockListByDevice.Get()))
    return false;
  if (IsDeviceBlocked(
          build_info->android_build_id(),
          kPassthroughCommandDecoderBlockListByAndroidBuildId.Get()))
    return false;
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kPassthroughCommandDecoderBlockListByManufacturer.Get()))
    return false;
  if (IsDeviceBlocked(build_info->model(),
                      kPassthroughCommandDecoderBlockListByModel.Get()))
    return false;
  if (IsDeviceBlocked(build_info->board(),
                      kPassthroughCommandDecoderBlockListByBoard.Get()))
    return false;
  if (IsDeviceBlocked(
          build_info->android_build_fp(),
          kPassthroughCommandDecoderBlockListByAndroidBuildFP.Get()))
    return false;
#endif  // defined(OS_ANDROID)

  return true;
}

}  // namespace features
