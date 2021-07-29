/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include <stdlib.h>

#include "av1/encoder/firstpass.h"
#include "av1/encoder/ratectrl.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

TEST(RatectrlTest, QModeGetQIndexTest) {
  int base_q_index = 36;
  int gf_update_type = INTNL_ARF_UPDATE;
  int gf_pyramid_level = 1;
  int arf_q = 100;
  int q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                       gf_pyramid_level, arf_q);
  EXPECT_EQ(q_index, arf_q);

  gf_update_type = INTNL_ARF_UPDATE;
  gf_pyramid_level = 3;
  q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                   gf_pyramid_level, arf_q);
  EXPECT_LT(q_index, arf_q);

  gf_update_type = LF_UPDATE;
  q_index = av1_q_mode_get_q_index(base_q_index, gf_update_type,
                                   gf_pyramid_level, arf_q);
  EXPECT_EQ(q_index, base_q_index);
}

TEST(RatectrlTest, QModeComputeGOPQIndicesTest) {
  int base_q_index = 36;
  int gfu_boost = 500;
  int bit_depth = AOM_BITS_8;
  double arf_boost_factor = 0.20;

  int gf_frame_index = 0;
  GF_GROUP gf_group = {};
  gf_group.size = 5;
  int layer_depth[5] = { 1, 3, 2, 3, 1 };
  int update_type[5] = { KF_UPDATE, INTNL_ARF_UPDATE, INTNL_OVERLAY_UPDATE,
                         INTNL_ARF_UPDATE, ARF_UPDATE };

  for (int i = 0; i < gf_group.size; i++) {
    gf_group.layer_depth[i] = layer_depth[i];
    gf_group.update_type[i] = update_type[i];
  }

  av1_q_mode_compute_gop_q_indices(gf_frame_index, base_q_index, gfu_boost,
                                   bit_depth, arf_boost_factor, &gf_group);

  for (int i = 0; i < gf_group.size; i++) {
    EXPECT_LE(gf_group.q_val[i], base_q_index);
  }
}

}  // namespace
