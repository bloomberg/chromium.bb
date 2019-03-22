// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "media/base/video_codecs.h"

namespace media {
namespace test {

// The video class provides functionality to load video files and manage their
// properties such as codec, number of frames,...
// TODO(@dstaessens):
// * Define and add functionality to load video properties. Each video should
//   have an accompanying json file containing: codec, number of frames,
//   MD5 checksums for the frame validator,... We could even go as far as
//   having an offset for each individual frame in the video
// * Use a file stream rather than loading potentially huge files into memory.
class Video {
 public:
  explicit Video(const base::FilePath& file_path);
  ~Video();

  // Load the video file from disk.
  bool Load();
  // Returns true if the video file was loaded.
  bool IsLoaded() const;

  // Get the video data, will be empty if the video hasn't been loaded yet.
  const std::vector<uint8_t>& GetData() const;

  // Get the video's codec.
  VideoCodecProfile GetProfile() const;

  // Set the default path to the test video data.
  static void SetTestDataPath(const base::FilePath& test_data_path);

 private:
  // The path where all test video files are stored.
  // TODO(dstaessens@) Avoid using a static data path here.
  static base::FilePath test_data_path_;
  // The video file's path, can be absolute or relative to the above path.
  base::FilePath file_path_;

  std::vector<uint8_t> data_;

  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(Video);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_H_
