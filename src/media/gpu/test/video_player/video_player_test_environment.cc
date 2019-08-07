// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include <utility>

#include "media/gpu/test/video_player/video.h"

namespace media {
namespace test {

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("test-25fps.h264");

// static
VideoPlayerTestEnvironment* VideoPlayerTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    bool enable_validator,
    bool output_frames,
    bool use_vd) {
  auto video = std::make_unique<media::test::Video>(
      video_path.empty() ? base::FilePath(kDefaultTestVideoPath) : video_path,
      video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  return new VideoPlayerTestEnvironment(std::move(video), enable_validator,
                                        output_frames, use_vd);
}

VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_validator,
    bool output_frames,
    bool use_vd)
    : video_(std::move(video)),
      enable_validator_(enable_validator),
      output_frames_(output_frames),
      use_vd_(use_vd) {}

VideoPlayerTestEnvironment::~VideoPlayerTestEnvironment() = default;

const media::test::Video* VideoPlayerTestEnvironment::Video() const {
  return video_.get();
}

bool VideoPlayerTestEnvironment::IsValidatorEnabled() const {
  return enable_validator_;
}

bool VideoPlayerTestEnvironment::IsFramesOutputEnabled() const {
  return output_frames_;
}

bool VideoPlayerTestEnvironment::UseVD() const {
  return use_vd_;
}

}  // namespace test
}  // namespace media
