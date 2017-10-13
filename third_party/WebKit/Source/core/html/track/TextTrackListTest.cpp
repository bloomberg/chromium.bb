// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/TextTrackList.h"

#include "core/html/media/HTMLVideoElement.h"
#include "core/html/track/TextTrack.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextTrackListTest, InvalidateTrackIndexes) {
  // Create and fill the list
  TextTrackList* list = TextTrackList::Create(
      HTMLVideoElement::Create(DummyPageHolder::Create()->GetDocument()));
  const size_t kNumTextTracks = 4;
  TextTrack* text_tracks[kNumTextTracks];
  for (size_t i = 0; i < kNumTextTracks; ++i) {
    text_tracks[i] = TextTrack::Create("subtitles", "", "");
    list->Append(text_tracks[i]);
  }

  EXPECT_EQ(4u, list->length());
  EXPECT_EQ(0, text_tracks[0]->TrackIndex());
  EXPECT_EQ(1, text_tracks[1]->TrackIndex());
  EXPECT_EQ(2, text_tracks[2]->TrackIndex());
  EXPECT_EQ(3, text_tracks[3]->TrackIndex());

  // Remove element from the middle of the list
  list->Remove(text_tracks[1]);

  EXPECT_EQ(3u, list->length());
  EXPECT_EQ(nullptr, text_tracks[1]->TrackList());
  EXPECT_EQ(0, text_tracks[0]->TrackIndex());
  EXPECT_EQ(1, text_tracks[2]->TrackIndex());
  EXPECT_EQ(2, text_tracks[3]->TrackIndex());
}

}  // namespace blink
