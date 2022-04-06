// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

#include "media/capabilities/webrtc_video_stats_db.h"

namespace media {
const auto MakeKey = WebrtcVideoStatsDB::VideoDescKey::MakeBucketedKey;
const auto ParsePixelsFromKey =
    WebrtcVideoStatsDB::VideoDescKey::ParsePixelsFromKey;

TEST(WebrtcVideoStatsDBTest, KeySerialization) {
  // Serialized key with empty KeySystem string should not mention encryption
  // fields.
  auto keyA = MakeKey(/*is_decode_stats=*/true, H264PROFILE_BASELINE,
                      /*hardware_accelerated=*/true, 1280 * 720);
  EXPECT_EQ("1|0|1|921600", keyA.Serialize());

  // No hardware acceleration.
  auto keyB = MakeKey(/*is_decode_stats=*/true, H264PROFILE_BASELINE,
                      /*hardware_accelerated=*/false, 1280 * 720);
  EXPECT_EQ("1|0|0|921600", keyB.Serialize());

  // VP9 Profile 2.
  auto keyC = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 1280 * 720);
  EXPECT_EQ("1|14|0|921600", keyC.Serialize());

  // Full HD.
  auto keyD = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 1920 * 1080);
  EXPECT_EQ("1|14|0|2073600", keyD.Serialize());

  // Encode.
  auto keyE = MakeKey(/*is_decode_stats=*/false, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 1920 * 1080);
  EXPECT_EQ("0|14|0|2073600", keyE.Serialize());

  // 4K.
  auto keyF = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 3840 * 2160);
  EXPECT_EQ("1|14|0|8294400", keyF.Serialize());
}

TEST(WebrtcVideoStatsDBTest, OperatorEquals) {
  auto keyA = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                      /*hardware_accelerated=*/true, 1280 * 720);
  EXPECT_EQ(keyA, keyA);

  auto keyB = keyA;
  EXPECT_EQ(keyA, keyB);

  // Vary each of the fields in `keyA` and verify != `keyA`
  EXPECT_NE(keyA, MakeKey(/*is_decode_stats=*/false, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/true, 1280 * 720));
  EXPECT_NE(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                          /*hardware_accelerated=*/true, 1280 * 720));
  EXPECT_NE(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/true, 1920 * 1080));
  EXPECT_NE(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/false, 1280 * 720));
}

TEST(WebrtcVideoStatsDBTest, PixelSizeBucketting) {
  auto keyA = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                      /*hardware_accelerated=*/true, 1280 * 720);

  // Verify that keys are equal even if the pixel size varies slightly.
  for (int pixel_size_delta = -10000; pixel_size_delta <= 10000;
       pixel_size_delta += 1000) {
    EXPECT_EQ(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                            /*hardware_accelerated=*/true,
                            1280 * 720 + pixel_size_delta));
  }

  // Even 0 and negative number of pixels are quantized to the first bucket.
  EXPECT_EQ(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/true,
                          /*pixels=*/0));
  EXPECT_EQ(keyA, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/true,
                          /*pixels=*/-10));

  // A high number of pixels is quantized to the last bucket.
  auto keyB = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                      /*hardware_accelerated=*/true, 3840 * 2160);
  EXPECT_EQ(keyB, MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE0,
                          /*hardware_accelerated=*/true,
                          /*pixels=*/1e9));
}

TEST(WebrtcVideoStatsDBTest, ParsePixelsFromKey) {
  // Valid formats on the form: "0|XX|1|YYY"
  // HD
  auto keyA = MakeKey(/*is_decode_stats=*/true, H264PROFILE_BASELINE,
                      /*hardware_accelerated=*/true, 1280 * 720);
  EXPECT_EQ(keyA.pixels, ParsePixelsFromKey(keyA.Serialize()));
  // Full HD.
  auto keyB = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 1920 * 1080);
  EXPECT_EQ(keyB.pixels, ParsePixelsFromKey(keyB.Serialize()));
  // 4K.
  auto keyC = MakeKey(/*is_decode_stats=*/true, VP9PROFILE_PROFILE2,
                      /*hardware_accelerated=*/false, 3840 * 2160);
  EXPECT_EQ(keyC.pixels, ParsePixelsFromKey(keyC.Serialize()));

  // Invalid key serializations.
  EXPECT_FALSE(ParsePixelsFromKey(""));
  EXPECT_FALSE(ParsePixelsFromKey("|||"));
  EXPECT_FALSE(ParsePixelsFromKey("|||123"));
  EXPECT_FALSE(ParsePixelsFromKey("0|1|0|"));
  EXPECT_FALSE(ParsePixelsFromKey("0|12|0|"));
}

}  // namespace media
