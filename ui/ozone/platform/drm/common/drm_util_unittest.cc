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

namespace {

// HP z32x monitor.
const unsigned char kHPz32x[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x32\x01\x01\x01\x01"
    "\x1B\x1B\x01\x04\xB5\x46\x27\x78\x3A\x8D\x15\xAC\x51\x32\xB8\x26"
    "\x0B\x50\x54\x21\x08\x00\xD1\xC0\xA9\xC0\x81\xC0\xD1\x00\xB3\x00"
    "\x95\x00\xA9\x40\x81\x80\x4D\xD0\x00\xA0\xF0\x70\x3E\x80\x30\x20"
    "\x35\x00\xB9\x88\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x18\x3C\x1E"
    "\x87\x3C\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x5A\x33\x32\x78\x0A\x20\x20\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x43\x37\x32\x37\x30\x4D\x57\x30\x0A\x20\x20\x01\x46"
    "\x02\x03\x18\xF1\x4B\x10\x1F\x04\x13\x03\x12\x02\x11\x01\x05\x14"
    "\x23\x09\x07\x07\x83\x01\x00\x00\xA3\x66\x00\xA0\xF0\x70\x1F\x80"
    "\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\x56\x5E\x00\xA0\xA0\xA0"
    "\x29\x50\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xEF\x51\x00\xA0"
    "\xF0\x70\x19\x80\x30\x20\x35\x00\xB9\x88\x21\x00\x00\x1A\xE2\x68"
    "\x00\xA0\xA0\x40\x2E\x60\x20\x30\x63\x00\xB9\x88\x21\x00\x00\x1C"
    "\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20\x36\x00\xB9\x88\x21\x00"
    "\x00\x1A\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x3E";

// Chromebook Samus internal display.
const unsigned char kSamus[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x30\xe4\x2e\x04\x00\x00\x00\x00"
    "\x00\x18\x01\x04\xa5\x1b\x12\x96\x02\x4f\xd5\xa2\x59\x52\x93\x26"
    "\x17\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x6d\x6f\x00\x9e\xa0\xa4\x31\x60\x30\x20"
    "\x3a\x00\x10\xb5\x10\x00\x00\x19\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfe\x00\x4c"
    "\x47\x20\x44\x69\x73\x70\x6c\x61\x79\x0a\x20\x20\x00\x00\x00\xfe"
    "\x00\x4c\x50\x31\x32\x39\x51\x45\x32\x2d\x53\x50\x41\x31\x00\x6c";

// Chromebook Eve internal display.
const unsigned char kEve[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4d\x10\x8a\x14\x00\x00\x00\x00"
    "\x16\x1b\x01\x04\xa5\x1a\x11\x78\x06\xde\x50\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xbb\x62\x60\xa0\x90\x40\x2e\x60\x30\x20"
    "\x3a\x00\x03\xad\x10\x00\x00\x18\x00\x00\x00\x10\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfc"
    "\x00\x4c\x51\x31\x32\x33\x50\x31\x4a\x58\x33\x32\x0a\x20\x00\xb6";

// Partially valid EDID: gamma information is marked as non existent.
const unsigned char kEdidWithNoGamma[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\xFF\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00";

// Invalid EDID: too short to contain chromaticity nor gamma information.
const unsigned char kInvalidEdid[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21";

// EDID collected in the wild: valid but with primaries in the wrong order.
const unsigned char kSST210[] =
    "\xff\x00\xff\xff\xff\xff\x00\xff\x2d\x4c\x42\x52\x32\x31\x50\x43"
    "\x0c\x2b\x03\x01\x33\x80\xa2\x20\x56\x2a\x9c\x4e\x50\x5b\x26\x95"
    "\x50\x23\xbf\x59\x80\xef\x80\x81\x59\x61\x59\x45\x59\x31\x40\x31"
    "\x01\x01\x01\x01\x01\x01\x32\x8c\xa0\x40\xb0\x60\x40\x19\x32\x1e"
    "\x00\x13\x44\x06\x00\x21\x1e\x00\x00\x00\xfd\x00\x38\x00\x1e\x55"
    "\x0f\x51\x0a\x00\x20\x20\x20\x20\x20\x20\x00\x00\xfc\x00\x32\x00"
    "\x30\x31\x20\x54\x69\x44\x69\x67\x61\x74\x0a\x6c\x00\x00\xff\x00"
    "\x48\x00\x4b\x34\x41\x54\x30\x30\x32\x38\x0a\x38\x20\x20\xf8\x00";

// EDID of |kSST210| with the order of the primaries corrected. Still invalid
// because the triangle of primaries is too small.
const unsigned char kSST210Corrected[] =
    "\xff\x00\xff\xff\xff\xff\x00\xff\x2d\x4c\x42\x52\x32\x31\x50\x43"
    "\x0c\x2b\x03\x01\x33\x80\xa2\x20\x56\x2a\x9c\x95\x50\x4e\x50\x5b"
    "\x26\x23\xbf\x59\x80\xef\x80\x81\x59\x61\x59\x45\x59\x31\x40\x31"
    "\x01\x01\x01\x01\x01\x01\x32\x8c\xa0\x40\xb0\x60\x40\x19\x32\x1e"
    "\x00\x13\x44\x06\x00\x21\x1e\x00\x00\x00\xfd\x00\x38\x00\x1e\x55"
    "\x0f\x51\x0a\x00\x20\x20\x20\x20\x20\x20\x00\x00\xfc\x00\x32\x00"
    "\x30\x31\x20\x54\x69\x44\x69\x67\x61\x74\x0a\x6c\x00\x00\xff\x00"
    "\x48\x00\x4b\x34\x41\x54\x30\x30\x32\x38\x0a\x38\x20\x20\xf8\x00";

}  // anonymous namespace

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
  EXPECT_EQ(a.color_space, b.color_space);
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

TEST_F(DrmUtilTest, RoundTripDisplayMode) {
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
  fp.color_space = gfx::ColorSpace::CreateREC709();
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
  sp.color_space = gfx::ColorSpace::CreateExtendedSRGB();
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
  ep.color_space = gfx::ColorSpace::CreateDisplayP3D65();
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

TEST_F(DrmUtilTest, GetColorSpaceFromEdid) {
  const std::vector<uint8_t> hpz32x_edid(kHPz32x,
                                         kHPz32x + arraysize(kHPz32x) - 1);
  const double hpz32x_toXYZ50_coeffs[] = {0.620465,   0.200003,  0.143752,  0.,
                                          0.290159,   0.667573,  0.0422682, 0.,
                                          0.00523514, 0.0673996, 0.752575,  0.};
  SkMatrix44 hpz32x_toXYZ50_matrix;
  hpz32x_toXYZ50_matrix.setRowMajord(hpz32x_toXYZ50_coeffs);
  const gfx::ColorSpace hpz32x_color_space = gfx::ColorSpace::CreateCustom(
      hpz32x_toXYZ50_matrix, SkColorSpaceTransferFn({2.2, 1, 0, 0, 0, 0, 0}));
  EXPECT_STREQ(hpz32x_color_space.ToString().c_str(),
               GetColorSpaceFromEdid(hpz32x_edid).ToString().c_str());

  const std::vector<uint8_t> samus_edid(kSamus, kSamus + arraysize(kSamus) - 1);
  const double samus_toXYZ50_coeffs[] = {0.41211,    0.39743,  0.15468,  0.,
                                         0.22317,    0.674029, 0.102801, 0.,
                                         0.00831561, 0.095953, 0.720941, 0.};
  SkMatrix44 samus_toXYZ50_matrix;
  samus_toXYZ50_matrix.setRowMajord(samus_toXYZ50_coeffs);
  const gfx::ColorSpace samus_color_space = gfx::ColorSpace::CreateCustom(
      samus_toXYZ50_matrix, SkColorSpaceTransferFn({2.5, 1, 0, 0, 0, 0, 0}));
  EXPECT_STREQ(samus_color_space.ToString().c_str(),
               GetColorSpaceFromEdid(samus_edid).ToString().c_str());

  const std::vector<uint8_t> eve_edid(kEve, kEve + arraysize(kEve) - 1);
  const double eve_toXYZ50_coeffs[] = {0.444601,  0.377972,  0.141646,  0.,
                                       0.226825,  0.713295,  0.0598808, 0.,
                                       0.0149676, 0.0972496, 0.712993,  0.};
  SkMatrix44 eve_toXYZ50_matrix;
  eve_toXYZ50_matrix.setRowMajord(eve_toXYZ50_coeffs);
  const gfx::ColorSpace eve_color_space = gfx::ColorSpace::CreateCustom(
      eve_toXYZ50_matrix, SkColorSpaceTransferFn({2.2, 1, 0, 0, 0, 0, 0}));
  EXPECT_STREQ(eve_color_space.ToString().c_str(),
               GetColorSpaceFromEdid(eve_edid).ToString().c_str());

  const std::vector<uint8_t> no_gamma_edid(
      kEdidWithNoGamma, kEdidWithNoGamma + arraysize(kEdidWithNoGamma) - 1);
  const gfx::ColorSpace no_gamma_color_space =
      GetColorSpaceFromEdid(no_gamma_edid);
  EXPECT_FALSE(no_gamma_color_space.IsValid());
}

TEST_F(DrmUtilTest, GetInvalidColorSpaceFromEdid) {
  const std::vector<uint8_t> empty_edid;
  EXPECT_EQ(gfx::ColorSpace(), GetColorSpaceFromEdid(empty_edid));

  const std::vector<uint8_t> invalid_edid(
      kInvalidEdid, kInvalidEdid + arraysize(kInvalidEdid) - 1);
  const gfx::ColorSpace invalid_color_space =
      GetColorSpaceFromEdid(invalid_edid);
  EXPECT_FALSE(invalid_color_space.IsValid());

  const std::vector<uint8_t> sst210_edid(kSST210,
                                         kSST210 + arraysize(kSST210) - 1);
  const gfx::ColorSpace sst210_color_space = GetColorSpaceFromEdid(sst210_edid);
  EXPECT_FALSE(sst210_color_space.IsValid()) << sst210_color_space.ToString();

  const std::vector<uint8_t> sst210_edid_2(
      kSST210Corrected, kSST210Corrected + arraysize(kSST210Corrected) - 1);
  const gfx::ColorSpace sst210_color_space_2 =
      GetColorSpaceFromEdid(sst210_edid_2);
  EXPECT_FALSE(sst210_color_space_2.IsValid())
      << sst210_color_space_2.ToString();
}

}  // namespace ui
