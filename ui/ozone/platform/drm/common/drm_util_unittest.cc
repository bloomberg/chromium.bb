// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <map>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

bool operator==(const ui::DisplayMode_Params& a,
                const ui::DisplayMode_Params& b) {
  return a.size == b.size && a.is_interlaced == b.is_interlaced &&
         a.refresh_rate == b.refresh_rate;
}

bool operator==(const ui::DisplaySnapshot_Params& a,
                const ui::DisplaySnapshot_Params& b) {
  return a.display_id == b.display_id && a.origin == b.origin &&
         a.physical_size == b.physical_size && a.type == b.type &&
         a.is_aspect_preserving_scaling == b.is_aspect_preserving_scaling &&
         a.has_overscan == b.has_overscan &&
         a.has_color_correction_matrix == b.has_color_correction_matrix &&
         a.display_name == b.display_name && a.sys_path == b.sys_path &&
         std::equal(a.modes.cbegin(), a.modes.cend(), b.modes.cbegin()) &&
         std::equal(a.edid.cbegin(), a.edid.cend(), a.edid.cbegin()) &&
         a.has_current_mode == b.has_current_mode &&
         a.current_mode == b.current_mode &&
         a.has_native_mode == b.has_native_mode &&
         a.native_mode == b.native_mode && a.product_id == b.product_id &&
         a.maximum_cursor_size == b.maximum_cursor_size;
}

namespace {

DisplayMode_Params MakeDisplay(float refresh) {
  DisplayMode_Params params;
  params.size = gfx::Size(101, 42);
  params.is_interlaced = true;
  params.refresh_rate = refresh;
  return params;
}

void DetailedCompare(const ui::DisplaySnapshot_Params& a,
                     const ui::DisplaySnapshot_Params& b) {
  EXPECT_EQ(a.display_id, b.display_id);
  EXPECT_EQ(a.origin, b.origin);
  EXPECT_EQ(a.physical_size, b.physical_size);
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.is_aspect_preserving_scaling, b.is_aspect_preserving_scaling);
  EXPECT_EQ(a.has_overscan, b.has_overscan);
  EXPECT_EQ(a.has_color_correction_matrix, b.has_color_correction_matrix);
  EXPECT_EQ(a.display_name, b.display_name);
  EXPECT_EQ(a.sys_path, b.sys_path);
  EXPECT_EQ(a.modes, b.modes);
  EXPECT_EQ(a.edid, b.edid);
  EXPECT_EQ(a.has_current_mode, b.has_current_mode);
  EXPECT_EQ(a.current_mode, b.current_mode);
  EXPECT_EQ(a.has_native_mode, b.has_native_mode);
  EXPECT_EQ(a.native_mode, b.native_mode);
  EXPECT_EQ(a.product_id, b.product_id);
  EXPECT_EQ(a.maximum_cursor_size, b.maximum_cursor_size);
}

}  // namespace

class DrmUtilTest : public testing::Test {};

TEST_F(DrmUtilTest, RoundTripEquivalence) {
  DisplayMode_Params orig_params = MakeDisplay(3.14);

  auto udm = CreateDisplayModeFromParams(orig_params);
  auto roundtrip_params = GetDisplayModeParams(*udm.get());

  EXPECT_EQ(orig_params.size.width(), roundtrip_params.size.width());
  EXPECT_EQ(orig_params.size.height(), roundtrip_params.size.height());
  EXPECT_EQ(orig_params.is_interlaced, roundtrip_params.is_interlaced);
  EXPECT_EQ(orig_params.refresh_rate, roundtrip_params.refresh_rate);
}

TEST_F(DrmUtilTest, RoundTripDisplaySnapshot) {
  std::vector<DisplaySnapshot_Params> orig_params;
  DisplaySnapshot_Params fp, sp, ep;

  fp.display_id = 101;
  fp.origin = gfx::Point(101, 42);
  fp.physical_size = gfx::Size(102, 43);
  fp.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  fp.is_aspect_preserving_scaling = true;
  fp.has_overscan = true;
  fp.has_color_correction_matrix = true;
  fp.display_name = "bending glass";
  fp.sys_path = base::FilePath("/bending");
  fp.modes =
      std::vector<DisplayMode_Params>({MakeDisplay(1.1), MakeDisplay(1.2)});
  fp.edid = std::vector<uint8_t>({'f', 'p'});
  fp.has_current_mode = true;
  fp.current_mode = MakeDisplay(1.2);
  fp.has_native_mode = true;
  fp.native_mode = MakeDisplay(1.1);
  fp.product_id = 7;
  fp.maximum_cursor_size = gfx::Size(103, 44);

  sp.display_id = 1002;
  sp.origin = gfx::Point(500, 42);
  sp.physical_size = gfx::Size(500, 43);
  sp.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  sp.is_aspect_preserving_scaling = true;
  sp.has_overscan = true;
  sp.has_color_correction_matrix = true;
  sp.display_name = "rigid glass";
  sp.sys_path = base::FilePath("/bending");
  sp.modes =
      std::vector<DisplayMode_Params>({MakeDisplay(500.1), MakeDisplay(500.2)});
  sp.edid = std::vector<uint8_t>({'s', 'p'});
  sp.has_current_mode = false;
  sp.has_native_mode = true;
  sp.native_mode = MakeDisplay(500.2);
  sp.product_id = 8;
  sp.maximum_cursor_size = gfx::Size(500, 44);

  ep.display_id = 2002;
  ep.origin = gfx::Point(1000, 42);
  ep.physical_size = gfx::Size(1000, 43);
  ep.type = display::DISPLAY_CONNECTION_TYPE_INTERNAL;
  ep.is_aspect_preserving_scaling = false;
  ep.has_overscan = false;
  ep.has_color_correction_matrix = false;
  ep.display_name = "fluted glass";
  ep.sys_path = base::FilePath("/bending");
  ep.modes = std::vector<DisplayMode_Params>(
      {MakeDisplay(1000.1), MakeDisplay(1000.2)});
  ep.edid = std::vector<uint8_t>({'s', 'p'});
  ep.has_current_mode = true;
  ep.current_mode = MakeDisplay(1000.2);
  ep.has_native_mode = false;
  ep.product_id = 9;
  ep.maximum_cursor_size = gfx::Size(1000, 44);

  orig_params.push_back(fp);
  orig_params.push_back(sp);
  orig_params.push_back(ep);

  MovableDisplaySnapshots intermediate_snapshots;
  for (const auto& snapshot_params : orig_params)
    intermediate_snapshots.push_back(CreateDisplaySnapshot(snapshot_params));

  std::vector<DisplaySnapshot_Params> roundtrip_params =
      CreateDisplaySnapshotParams(intermediate_snapshots);

  DetailedCompare(fp, roundtrip_params[0]);

  EXPECT_EQ(fp, roundtrip_params[0]);
  EXPECT_EQ(sp, roundtrip_params[1]);
  EXPECT_EQ(ep, roundtrip_params[2]);
}

TEST_F(DrmUtilTest, OverlaySurfaceCandidate) {
  OverlaySurfaceCandidateList input;

  OverlaySurfaceCandidate input_osc;
  input_osc.transform = gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  input_osc.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  input_osc.buffer_size = gfx::Size(100, 50);
  input_osc.display_rect = gfx::RectF(1., 2., 3., 4.);
  input_osc.crop_rect = gfx::RectF(10., 20., 30., 40.);
  input_osc.clip_rect = gfx::Rect(10, 20, 30, 40);
  input_osc.is_clipped = true;
  input_osc.plane_z_order = 42;
  input_osc.overlay_handled = true;

  input.push_back(input_osc);

  // Roundtrip the conversions.
  auto output = CreateOverlaySurfaceCandidateListFrom(
      CreateParamsFromOverlaySurfaceCandidate(input));

  EXPECT_EQ(input.size(), output.size());
  OverlaySurfaceCandidate output_osc = output[0];

  EXPECT_EQ(input_osc.transform, output_osc.transform);
  EXPECT_EQ(input_osc.format, output_osc.format);
  EXPECT_EQ(input_osc.buffer_size, output_osc.buffer_size);
  EXPECT_EQ(input_osc.display_rect, output_osc.display_rect);
  EXPECT_EQ(input_osc.crop_rect, output_osc.crop_rect);
  EXPECT_EQ(input_osc.plane_z_order, output_osc.plane_z_order);
  EXPECT_EQ(input_osc.overlay_handled, output_osc.overlay_handled);

  EXPECT_FALSE(input < output);
  EXPECT_FALSE(output < input);

  std::map<OverlaySurfaceCandidateList, int> map;
  map[input] = 42;
  const auto& iter = map.find(output);

  EXPECT_NE(map.end(), iter);
  EXPECT_EQ(42, iter->second);
}

}  // namespace ui
