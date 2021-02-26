/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Tests for https://crbug.com/aomedia/2777.
//
// Encode images with a small width (<= two AV1 superblocks) or a small height
// (<= one AV1 superblock) with multiple threads. aom_codec_encode() should
// not crash.

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "aom/aomcx.h"
#include "aom/aom_encoder.h"

namespace {

// Dummy buffer of zero samples.
constexpr unsigned char kBuffer[256 * 512 + 2 * 128 * 256] = { 0 };

TEST(EncodeSmallWidthHeight, SmallWidthMultiThreaded) {
  // The image has only one tile and the tile is two AV1 superblocks wide.
  // For speed >= 1, superblock size is 64x64 (see av1_select_sb_size()).
  constexpr int kWidth = 128;
  constexpr int kHeight = 512;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, NULL, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallWidthMultiThreadedSpeed0) {
  // The image has only one tile and the tile is two AV1 superblocks wide.
  // For speed 0, superblock size is 128x128 (see av1_select_sb_size()).
  constexpr int kWidth = 256;
  constexpr int kHeight = 512;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, NULL, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallHeightMultiThreaded) {
  // The image has only one tile and the tile is one AV1 superblock tall.
  // For speed >= 1, superblock size is 64x64 (see av1_select_sb_size()).
  constexpr int kWidth = 512;
  constexpr int kHeight = 64;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 5));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, NULL, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(EncodeSmallWidthHeight, SmallHeightMultiThreadedSpeed0) {
  // The image has only one tile and the tile is one AV1 superblock tall.
  // For speed 0, superblock size is 128x128 (see av1_select_sb_size()).
  constexpr int kWidth = 512;
  constexpr int kHeight = 128;

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               const_cast<unsigned char *>(kBuffer)));

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(iface, &cfg, 0));
  cfg.g_threads = 2;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, iface, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, NULL, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

}  // namespace
