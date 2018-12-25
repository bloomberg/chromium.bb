// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stdint.h>

#include <memory>

#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace display {

namespace {

// Sample EDID data extracted from real devices.
constexpr unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";
constexpr size_t kNormalDisplayLength = base::size(kNormalDisplay);

constexpr unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";
constexpr size_t kInternalDisplayLength = base::size(kInternalDisplay);

constexpr unsigned char kOverscanDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xfe\x08\x00\x00\x00\x00"
    "\x29\x15\x01\x03\x80\x10\x09\x78\x0a\xee\x91\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
    "\x45\x00\xa0\x5a\x00\x00\x00\x1e\x66\x21\x56\xaa\x51\x00\x1e\x30"
    "\x46\x8f\x33\x00\xa0\x5a\x00\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x17\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x1d"
    "\x02\x03\x1f\xf1\x47\x90\x04\x05\x03\x20\x22\x07\x23\x09\x07\x07"
    "\x83\x01\x00\x00\xe2\x00\x0f\x67\x03\x0c\x00\x20\x00\xb8\x2d\x01"
    "\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00\xa0\x5a\x00\x00\x00"
    "\x9e\x01\x1d\x00\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\xa0\x5a\x00"
    "\x00\x00\x1e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\xa0"
    "\x5a\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc6";
constexpr size_t kOverscanDisplayLength = base::size(kOverscanDisplay);

// The EDID info misdetecting overscan once. see crbug.com/226318
constexpr unsigned char kMisdetectedDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x64\x40\x4c\x30\x30\x32"
    "\x0c\x15\x01\x03\x80\x40\x28\x78\xea\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\xa5\x4b\x00\x71\x4f\x81\x00\x81\x80\xd1\x00\xa9\x40"
    "\x01\x01\x01\x01\x01\x01\x28\x3c\x80\xa0\x70\xb0\x23\x40\x30\x20"
    "\x36\x00\x81\x91\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x50\x48\x35"
    "\x4e\x59\x31\x33\x4e\x32\x30\x30\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x33\x30\x31\x31\x0a\x20\x20\x00\x00\x00\xfd"
    "\x00\x31\x56\x1d\x5e\x12\x00\x0a\x20\x20\x20\x20\x20\x20\x01\x38"
    "\x02\x03\x29\xf1\x50\x90\x05\x04\x03\x02\x07\x16\x01\x06\x11\x12"
    "\x15\x13\x14\x1f\x20\x23\x0d\x7f\x07\x83\x0f\x00\x00\x67\x03\x0c"
    "\x00\x10\x00\x38\x2d\xe3\x05\x03\x01\x02\x3a\x80\x18\x71\x38\x2d"
    "\x40\x58\x2c\x45\x00\x81\x91\x21\x00\x00\x1e\x01\x1d\x80\x18\x71"
    "\x1c\x16\x20\x58\x2c\x25\x00\x81\x91\x21\x00\x00\x9e\x01\x1d\x00"
    "\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\x81\x91\x21\x00\x00\x1e\x8c"
    "\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\x81\x91\x21\x00\x00"
    "\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x94";
constexpr size_t kMisdetectedDisplayLength = base::size(kMisdetectedDisplay);

constexpr unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";
constexpr size_t kLP2565ALength = base::size(kLP2565A);

constexpr unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";
constexpr size_t kLP2565BLength = base::size(kLP2565B);

// HP z32x monitor.
constexpr unsigned char kHPz32x[] =
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
constexpr size_t kHPz32xLength = base::size(kHPz32x);

// Chromebook Samus internal display.
constexpr unsigned char kSamus[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x30\xe4\x2e\x04\x00\x00\x00\x00"
    "\x00\x18\x01\x04\xa5\x1b\x12\x96\x02\x4f\xd5\xa2\x59\x52\x93\x26"
    "\x17\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x6d\x6f\x00\x9e\xa0\xa4\x31\x60\x30\x20"
    "\x3a\x00\x10\xb5\x10\x00\x00\x19\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfe\x00\x4c"
    "\x47\x20\x44\x69\x73\x70\x6c\x61\x79\x0a\x20\x20\x00\x00\x00\xfe"
    "\x00\x4c\x50\x31\x32\x39\x51\x45\x32\x2d\x53\x50\x41\x31\x00\x6c";
constexpr size_t kSamusLength = base::size(kSamus);

// Chromebook Eve internal display.
constexpr unsigned char kEve[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4d\x10\x8a\x14\x00\x00\x00\x00"
    "\x16\x1b\x01\x04\xa5\x1a\x11\x78\x06\xde\x50\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xbb\x62\x60\xa0\x90\x40\x2e\x60\x30\x20"
    "\x3a\x00\x03\xad\x10\x00\x00\x18\x00\x00\x00\x10\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfc"
    "\x00\x4c\x51\x31\x32\x33\x50\x31\x4a\x58\x33\x32\x0a\x20\x00\xb6";
constexpr size_t kEveLength = base::size(kEve);

// Primaries coordinates ({RX, RY, GX, GY, BX, BY, WX, WY}) calculated by hand
// and rounded to 4 decimal places.
constexpr SkColorSpacePrimaries kNormalDisplayPrimaries = {
    0.6777f, 0.3086f, 0.2100f, 0.6924f, 0.1465f, 0.0547f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kInternalDisplayPrimaries = {
    0.5850f, 0.3604f, 0.3750f, 0.5654f, 0.1553f, 0.0996f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kOverscanDisplayPrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.5996f, 0.1504f, 0.0596f, 0.3125f, 0.3291f};
constexpr SkColorSpacePrimaries kMisdetectedDisplayPrimaries = {
    0.6777f, 0.3086f, 0.2100f, 0.6924f, 0.1465f, 0.0547f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kLP2565APrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.6084f, 0.1504f, 0.0596f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kLP2565BPrimaries = {
    0.6396f, 0.3301f, 0.2998f, 0.6084f, 0.1504f, 0.0596f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kHPz32xPrimaries = {
    0.6738f, 0.3164f, 0.1982f, 0.7197f, 0.1484f, 0.0439f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kSamusPrimaries = {
    0.6338f, 0.3477f, 0.3232f, 0.5771f, 0.1514f, 0.0908f, 0.3135f, 0.3291f};
constexpr SkColorSpacePrimaries kEvePrimaries = {
    0.6396f, 0.3291f, 0.2998f, 0.5996f, 0.1494f, 0.0596f, 0.3125f, 0.3281f};

// Chromaticity primaries in EDID are specified with 10 bits precision.
constexpr static float kPrimariesPrecision = 1 / 2048.f;

::testing::AssertionResult SkColorSpacePrimariesEquals(
    const char* lhs_expr,
    const char* rhs_expr,
    const SkColorSpacePrimaries& lhs,
    const SkColorSpacePrimaries& rhs) {
  if (!base::IsApproximatelyEqual(lhs.fRX, rhs.fRX, kPrimariesPrecision))
    return AssertionFailure() << "fRX: " << lhs.fRX << " != " << rhs.fRX;
  if (!base::IsApproximatelyEqual(lhs.fRY, rhs.fRY, kPrimariesPrecision))
    return AssertionFailure() << "fRY: " << lhs.fRY << " != " << rhs.fRY;
  if (!base::IsApproximatelyEqual(lhs.fGX, rhs.fGX, kPrimariesPrecision))
    return AssertionFailure() << "fGX: " << lhs.fGX << " != " << rhs.fGX;
  if (!base::IsApproximatelyEqual(lhs.fGY, rhs.fGY, kPrimariesPrecision))
    return AssertionFailure() << "fGY: " << lhs.fGY << " != " << rhs.fGY;
  if (!base::IsApproximatelyEqual(lhs.fBX, rhs.fBX, kPrimariesPrecision))
    return AssertionFailure() << "fBX: " << lhs.fBX << " != " << rhs.fBX;
  if (!base::IsApproximatelyEqual(lhs.fBY, rhs.fBY, kPrimariesPrecision))
    return AssertionFailure() << "fBY: " << lhs.fBY << " != " << rhs.fBY;
  if (!base::IsApproximatelyEqual(lhs.fWX, rhs.fWX, kPrimariesPrecision))
    return AssertionFailure() << "fWX: " << lhs.fWX << " != " << rhs.fWX;
  if (!base::IsApproximatelyEqual(lhs.fWY, rhs.fWY, kPrimariesPrecision))
    return AssertionFailure() << "fWY: " << lhs.fWY << " != " << rhs.fWY;
  return AssertionSuccess();
}

}  // namespace

struct TestParams {
  uint16_t manufacturer_id;
  uint16_t product_id;
  std::string display_name;
  gfx::Size active_pixel_size;
  int32_t year_of_manufacture;
  bool overscan_flag;
  double gamma;
  int bits_per_channel;
  SkColorSpacePrimaries primaries;

  uint32_t product_code;
  int64_t display_id_zero;

  std::string manufacturer_id_string;
  std::string product_id_string;

  const unsigned char* edid_blob;
  size_t edid_blob_length;
} kTestCases[] = {
    {0x22f0u, 0x6c28u, "HP ZR30w", gfx::Size(2560, 1600), 2012, false, 2.2, 10,
     kNormalDisplayPrimaries, 586181672, 9834734971736576, "HWP", "286C",
     kNormalDisplay, kNormalDisplayLength},
    {0x4ca3u, 0x4231u, "", gfx::Size(1280, 800), 2011, false, 2.2, -1,
     kInternalDisplayPrimaries, 1285767729, 21571318625337344, "SEC", "3142",
     kInternalDisplay, kInternalDisplayLength},
    {0x4c2du, 0xfe08u, "SAMSUNG", gfx::Size(1920, 1080), 2011, true, 2.2, -1,
     kOverscanDisplayPrimaries, 1278082568, 21442559853606400, "SAM", "08FE",
     kOverscanDisplay, kOverscanDisplayLength},
    {0x10ACu, 0x6440u, "DELL U3011", gfx::Size(1920, 1200), 2011, false, 2.2,
     -1, kMisdetectedDisplayPrimaries, 279733312, 4692848143772416, "DEL",
     "4064", kMisdetectedDisplay, kMisdetectedDisplayLength},
    {0x22f0u, 0x7626u, "HP LP2465", gfx::Size(1920, 1200), 2008, false, 2.2, -1,
     kLP2565APrimaries, 586184230, 9834630174887424, "HWP", "2676", kLP2565A,
     kLP2565ALength},
    {0x22f0u, 0x7526u, "HP LP2465", gfx::Size(1920, 1200), 2008, false, 2.2, -1,
     kLP2565BPrimaries, 586183974, 9834630174887424, "HWP", "2675", kLP2565B,
     kLP2565BLength},
    {0x22f0u, 0x7532u, "HP Z32x", gfx::Size(3840, 2160), 2017, false, 2.2, 10,
     kHPz32xPrimaries, 586183986, 9834799315992832, "HWP", "3275", kHPz32x,
     kHPz32xLength},
    {0x30E4u, 0x2E04u, "", gfx::Size(2560, 1700), 2014, false, 2.5, 8,
     kSamusPrimaries, 820260356, 13761487533244416, "LGD", "042E", kSamus,
     kSamusLength},
    {0x4D10u, 0x8A14u, "LQ123P1JX32", gfx::Size(2400, 1600), 2017, false, 2.2,
     8, kEvePrimaries, 1292929556, 21692109949126656, "SHP", "148A", kEve,
     kEveLength},

    // Empty Edid, which is tantamount to error.
    {0, 0, "", gfx::Size(0, 0), display::kInvalidYearOfManufacture, false, 0.0,
     -1, SkColorSpacePrimaries(), 0, 0, "@@@", "0000", nullptr, 0u},
};

class EDIDParserTest : public TestWithParam<TestParams> {
 public:
  EDIDParserTest()
      : parser_(std::vector<uint8_t>(
            GetParam().edid_blob,
            GetParam().edid_blob + GetParam().edid_blob_length)) {}

  const EdidParser parser_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EDIDParserTest);
};

TEST_P(EDIDParserTest, ParseEdids) {
  EXPECT_EQ(parser_.manufacturer_id(), GetParam().manufacturer_id);
  EXPECT_EQ(parser_.product_id(), GetParam().product_id);
  EXPECT_EQ(parser_.display_name(), GetParam().display_name);
  EXPECT_EQ(parser_.active_pixel_size(), GetParam().active_pixel_size);
  EXPECT_EQ(parser_.year_of_manufacture(), GetParam().year_of_manufacture);
  EXPECT_EQ(parser_.has_overscan_flag(), GetParam().overscan_flag);
  if (parser_.has_overscan_flag())
    EXPECT_EQ(parser_.overscan_flag(), GetParam().overscan_flag);
  EXPECT_DOUBLE_EQ(parser_.gamma(), GetParam().gamma);
  EXPECT_EQ(parser_.bits_per_channel(), GetParam().bits_per_channel);
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, parser_.primaries(),
                      GetParam().primaries);

  EXPECT_EQ(parser_.GetProductCode(), GetParam().product_code);
  EXPECT_EQ(parser_.GetDisplayId(0 /* product_index */),
            GetParam().display_id_zero);

  EXPECT_EQ(EdidParser::ManufacturerIdToString(parser_.manufacturer_id()),
            GetParam().manufacturer_id_string);
  EXPECT_EQ(EdidParser::ProductIdToString(parser_.product_id()),
            GetParam().product_id_string);
}

INSTANTIATE_TEST_CASE_P(, EDIDParserTest, ValuesIn(kTestCases));

}  // namespace display
