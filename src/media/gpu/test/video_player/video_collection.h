// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_COLLECTION_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_COLLECTION_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/macros.h"

namespace media {
namespace test {

class Video;

// The video collection class helps managing different sets of test videos.
// Multiple test video collections can be maintained:
// * A collection of lightweight videos for CQ testing.
// * A collection of large video files for performance testing.
// * A set of corrupt videos to test decoder stability.
// * A set of small generated video files with various properties.
// TODO(dstaessens@):
// * Add functionality to fetch video by codec/resolution/name/...
// * Add a collection of videos to test different codecs.
// * Add a collection of videos to test various resolutions.
// * Add a collection of lightweight videos (defined directly in code?).
class VideoCollection {
 public:
  VideoCollection();
  ~VideoCollection();
  VideoCollection(VideoCollection&& other);

  // Add a video to the collection, this will transfer ownership.
  VideoCollection& Add(std::unique_ptr<Video> video);

  // Get the video with specified index from the collection.
  const Video& operator[](size_t index) const;

  size_t Size() const;

 private:
  std::vector<std::unique_ptr<Video>> video_collection_;

  DISALLOW_COPY_AND_ASSIGN(VideoCollection);
};

// The default video test file collection
extern const VideoCollection kDefaultTestVideoCollection;

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_COLLECTION_H_
