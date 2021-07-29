// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/feed_feature_list.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(ProtoUtilTest, CreateClientInfo) {
  RequestMetadata request_metadata;
  request_metadata.chrome_info.version = base::Version({1, 2, 3, 4});
  request_metadata.chrome_info.channel = version_info::Channel::STABLE;
  request_metadata.display_metrics.density = 1;
  request_metadata.display_metrics.width_pixels = 2;
  request_metadata.display_metrics.height_pixels = 3;
  request_metadata.language_tag = "en-US";

  feedwire::ClientInfo result = CreateClientInfo(request_metadata);
  EXPECT_EQ(feedwire::ClientInfo::CHROME_ANDROID, result.app_type());
  EXPECT_EQ(feedwire::Version::RELEASE, result.app_version().build_type());
  EXPECT_EQ(1, result.app_version().major());
  EXPECT_EQ(2, result.app_version().minor());
  EXPECT_EQ(3, result.app_version().build());
  EXPECT_EQ(4, result.app_version().revision());

  EXPECT_EQ(R"({
  screen_density: 1
  screen_width_in_pixels: 2
  screen_height_in_pixels: 3
}
)",
            ToTextProto(result.display_info(0)));
  EXPECT_EQ("en-US", result.locale());
}

TEST(ProtoUtilTest, DefaultCapabilities) {
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(kForYouStream,
                                    feedwire::FeedQuery::MANUAL_REFRESH,
                                    /*request_metadata=*/{},
                                    /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_THAT(
      request.client_capability(),
      testing::UnorderedElementsAre(
          feedwire::Capability::BASE_UI, feedwire::Capability::REQUEST_SCHEDULE,
          feedwire::Capability::LOTTIE_ANIMATIONS,
          feedwire::Capability::LONG_PRESS_CARD_MENU,
          feedwire::Capability::OPEN_IN_TAB, feedwire::Capability::CARD_MENU,
          feedwire::Capability::DOWNLOAD_LINK,
          feedwire::Capability::INFINITE_FEED,
          feedwire::Capability::DISMISS_COMMAND,
          feedwire::Capability::MATERIAL_NEXT_BASELINE,
          feedwire::Capability::UI_THEME_V2,
          feedwire::Capability::UNDO_FOR_DISMISS_COMMAND,
          feedwire::Capability::PREFETCH_METADATA,
          feedwire::Capability::SHARE));
}

TEST(ProtoUtilTest, HeartsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kInterestFeedV2Hearts}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(kForYouStream,
                                    feedwire::FeedQuery::MANUAL_REFRESH,
                                    /*request_metadata=*/{},
                                    /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              testing::Contains(feedwire::Capability::HEART));
}

TEST(ProtoUtilTest, DisableCapabilitiesWithFinch) {
  // Try to disable BASE_UI and _INFINITE_FEED. BASE_UI is not an experimental
  // capability, and should not be affected.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedV2,
      {{"enable_BASE_UI", "false"}, {"enable_INFINITE_FEED", "false"}});
  OverrideConfigWithFinchForTesting();

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(kForYouStream,
                                    feedwire::FeedQuery::MANUAL_REFRESH,
                                    /*request_metadata=*/{},
                                    /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_THAT(
      request.client_capability(),
      testing::UnorderedElementsAre(
          feedwire::Capability::BASE_UI, feedwire::Capability::REQUEST_SCHEDULE,
          feedwire::Capability::LOTTIE_ANIMATIONS,
          feedwire::Capability::LONG_PRESS_CARD_MENU,
          feedwire::Capability::OPEN_IN_TAB, feedwire::Capability::CARD_MENU,
          feedwire::Capability::DOWNLOAD_LINK,
          feedwire::Capability::DISMISS_COMMAND, feedwire::Capability::SHARE,
          feedwire::Capability::MATERIAL_NEXT_BASELINE,
          feedwire::Capability::UI_THEME_V2,
          feedwire::Capability::UNDO_FOR_DISMISS_COMMAND,
          feedwire::Capability::PREFETCH_METADATA));
}

TEST(ProtoUtilTest, NoticeCardAcknowledged) {
  RequestMetadata request_metadata;
  request_metadata.notice_card_acknowledged = true;
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      kForYouStream, feedwire::FeedQuery::MANUAL_REFRESH, request_metadata,
      /*consistency_token=*/std::string());

  EXPECT_TRUE(request.feed_request()
                  .feed_query()
                  .chrome_fulfillment_info()
                  .notice_card_acknowledged());
}

TEST(ProtoUtilTest, NoticeCardNotAcknowledged) {
  RequestMetadata request_metadata;
  request_metadata.notice_card_acknowledged = false;
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      kForYouStream, feedwire::FeedQuery::MANUAL_REFRESH, request_metadata,
      /*consistency_token=*/std::string());

  EXPECT_FALSE(request.feed_request()
                   .feed_query()
                   .chrome_fulfillment_info()
                   .notice_card_acknowledged());
}

TEST(ProtoUtilTest, AutoplayEnabled) {
  RequestMetadata request_metadata;
  request_metadata.autoplay_enabled = true;

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          kForYouStream, feedwire::FeedQuery::MANUAL_REFRESH, request_metadata,
          /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              testing::Contains(feedwire::Capability::INLINE_VIDEO_AUTOPLAY));
  ASSERT_THAT(request.client_capability(),
              testing::Contains(feedwire::Capability::OPEN_VIDEO_COMMAND));
}

}  // namespace
}  // namespace feed
