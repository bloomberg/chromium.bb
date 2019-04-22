// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_sink.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

TEST(MediaSinkTest, IsMaybeCloudSink) {
  MediaSink meeting("sinkId", "Sink", SinkIconType::MEETING,
                    MediaRouteProviderId::EXTENSION);
  MediaSink eduReceiver("sinkId2", "Sink", SinkIconType::EDUCATION,
                        MediaRouteProviderId::EXTENSION);
  MediaSink chromeCast("sinkId3", "Sink", SinkIconType::CAST,
                       MediaRouteProviderId::EXTENSION);

  EXPECT_TRUE(meeting.IsMaybeCloudSink());
  EXPECT_TRUE(eduReceiver.IsMaybeCloudSink());
  EXPECT_FALSE(chromeCast.IsMaybeCloudSink());
}

TEST(MediaSinkTest, TestEquals) {
  MediaSink sink1("sinkId", "Sink", SinkIconType::CAST,
                  MediaRouteProviderId::EXTENSION);

  MediaSink sink1_copy(sink1);
  EXPECT_EQ(sink1, sink1_copy);

  // No name.
  MediaSink sink2("sinkId", "", SinkIconType::CAST,
                  MediaRouteProviderId::EXTENSION);
  EXPECT_FALSE(sink1 == sink2);

  // Sink name is different from sink1's.
  MediaSink sink3("sinkId", "Other Sink", SinkIconType::CAST,
                  MediaRouteProviderId::EXTENSION);
  EXPECT_FALSE(sink1 == sink3);

  // Sink ID is diffrent from sink1's.
  MediaSink sink4("otherSinkId", "Sink", SinkIconType::CAST,
                  MediaRouteProviderId::EXTENSION);
  EXPECT_FALSE(sink1 == sink4);

  // Sink icon type is diffrent from sink1's.
  MediaSink sink5("otherSinkId", "Sink", SinkIconType::GENERIC,
                  MediaRouteProviderId::EXTENSION);
  EXPECT_FALSE(sink1 == sink5);
}

}  // namespace media_router
