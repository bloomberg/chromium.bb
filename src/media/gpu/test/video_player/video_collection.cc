// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_collection.h"

#include <utility>

#include "media/gpu/test/video_player/video.h"

namespace media {
namespace test {

constexpr base::FilePath::CharType kTestVideoH264[] = "test-25fps.h264";
constexpr base::FilePath::CharType kTestVideoVP8[] = "test-25fps.vp8";
constexpr base::FilePath::CharType kTestVideoVP9[] = "test-25fps.vp9";

const VideoCollection kDefaultTestVideoCollection =
    std::move(VideoCollection()
                  .Add(std::make_unique<Video>(base::FilePath(kTestVideoH264)))
                  .Add(std::make_unique<Video>(base::FilePath(kTestVideoVP8)))
                  .Add(std::make_unique<Video>(base::FilePath(kTestVideoVP9))));

VideoCollection::VideoCollection() {}

VideoCollection::~VideoCollection() {}

VideoCollection::VideoCollection(VideoCollection&& other) {
  video_collection_ = std::move(other.video_collection_);
}

VideoCollection& VideoCollection::Add(std::unique_ptr<Video> video) {
  video_collection_.push_back(std::move(video));
  return *this;
}

const Video& VideoCollection::operator[](size_t index) const {
  CHECK_LT(index, video_collection_.size());
  Video* video = video_collection_.at(index).get();

  // Only load video when actually requested, to avoid loading all videos in the
  // collection, even when only a single video is needed.
  if (!video->IsLoaded()) {
    bool loaded = video->Load();
    LOG_IF(FATAL, !loaded) << "Loading video failed";
  }

  return *video;
}

size_t VideoCollection::Size() const {
  return video_collection_.size();
}

}  // namespace test
}  // namespace media
