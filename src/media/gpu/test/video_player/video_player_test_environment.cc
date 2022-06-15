// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/system/sys_info.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"

namespace media {
namespace test {

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("test-25fps.h264");

// static
VideoPlayerTestEnvironment* VideoPlayerTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    ValidatorType validator_type,
    const DecoderImplementation implementation,
    bool linear_output,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::Feature>& enabled_features,
    const std::vector<base::Feature>& disabled_features) {
  auto video = std::make_unique<media::test::Video>(
      video_path.empty() ? base::FilePath(kDefaultTestVideoPath) : video_path,
      video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  // TODO(b/182008564) Add checks to make sure no features are duplicated, and
  // there is no intersection between the enabled and disabled set.
  std::vector<base::Feature> combined_enabled_features(enabled_features);
  combined_enabled_features.push_back(media::kVp9kSVCHWDecoding);
  std::vector<base::Feature> combined_disabled_features(disabled_features);
#if BUILDFLAG(USE_VAAPI)
  // TODO(b/172217032): remove once enabled by default.
  combined_enabled_features.push_back(media::kVaapiAV1Decoder);

  // Disable this feature so that the decoder test can test a
  // resolution which is denied for the sake of performance. See
  // b/171041334.
  combined_disabled_features.push_back(
      media::kVaapiEnforceVideoMinMaxResolution);
#endif

  return new VideoPlayerTestEnvironment(
      std::move(video), validator_type, implementation, linear_output,
      output_folder, frame_output_config, combined_enabled_features,
      combined_disabled_features);
}

VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    ValidatorType validator_type,
    const DecoderImplementation implementation,
    bool linear_output,
    const base::FilePath& output_folder,
    const FrameOutputConfig& frame_output_config,
    const std::vector<base::Feature>& enabled_features,
    const std::vector<base::Feature>& disabled_features)
    : VideoTestEnvironment(enabled_features, disabled_features),
      video_(std::move(video)),
      validator_type_(validator_type),
      implementation_(implementation),
      linear_output_(linear_output),
      frame_output_config_(frame_output_config),
      output_folder_(output_folder) {}

VideoPlayerTestEnvironment::~VideoPlayerTestEnvironment() = default;

const media::test::Video* VideoPlayerTestEnvironment::Video() const {
  return video_.get();
}

bool VideoPlayerTestEnvironment::IsValidatorEnabled() const {
  return validator_type_ != ValidatorType::kNone;
}

VideoPlayerTestEnvironment::ValidatorType
VideoPlayerTestEnvironment::GetValidatorType() const {
  return validator_type_;
}

DecoderImplementation VideoPlayerTestEnvironment::GetDecoderImplementation()
    const {
  return implementation_;
}

bool VideoPlayerTestEnvironment::ShouldOutputLinearBuffers() const {
  return linear_output_;
}

FrameOutputMode VideoPlayerTestEnvironment::GetFrameOutputMode() const {
  return frame_output_config_.output_mode;
}

VideoFrameFileWriter::OutputFormat
VideoPlayerTestEnvironment::GetFrameOutputFormat() const {
  return frame_output_config_.output_format;
}

uint64_t VideoPlayerTestEnvironment::GetFrameOutputLimit() const {
  return frame_output_config_.output_limit;
}

const base::FilePath& VideoPlayerTestEnvironment::OutputFolder() const {
  return output_folder_;
}

}  // namespace test
}  // namespace media
