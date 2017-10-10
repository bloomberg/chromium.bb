// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/size.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;

namespace display {

namespace {

// Returns the number of characters in the string literal but doesn't count its
// terminator NULL byte.
#define charsize(str) (arraysize(str) - 1)

// Sample EDID data extracted from real devices.
const unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";

const unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";

const unsigned char kOverscanDisplay[] =
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

// The EDID info misdetecting overscan once. see crbug.com/226318
const unsigned char kMisdetectedDisplay[] =
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

const unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";

const unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";

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

void Reset(gfx::Size* pixel, gfx::Size* size) {
  pixel->SetSize(0, 0);
  size->SetSize(0, 0);
}
// Chromaticity primaries in EDID are specified with 10 bits precision.
const static float kPrimariesPrecision = 0.001f;

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

TEST(EDIDParserTest, ParseOverscanFlag) {
  bool flag = false;
  std::vector<uint8_t> edid(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  edid.assign(kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  EXPECT_TRUE(ParseOutputOverscanFlag(edid, &flag));
  EXPECT_TRUE(flag);

  flag = false;
  edid.assign(
      kMisdetectedDisplay, kMisdetectedDisplay + charsize(kMisdetectedDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  // Copy |kOverscanDisplay| and set flags to false in it. The overscan flags
  // are embedded at byte 150 in this specific example. Fix here too when the
  // contents of kOverscanDisplay is altered.
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  edid[150] = '\0';
  EXPECT_TRUE(ParseOutputOverscanFlag(edid, &flag));
  EXPECT_FALSE(flag);
}

TEST(EDIDParserTest, ParseBrokenOverscanData) {
  // Do not fill valid data here because it anyway fails to parse the data.
  std::vector<uint8_t> data;
  bool flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));
  data.assign(126, '\0');
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));

  // extending data because ParseOutputOverscanFlag() will access the data.
  data.assign(128, '\0');
  // The number of CEA extensions is stored at byte 126.
  data[126] = '\x01';
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));

  data.assign(150, '\0');
  data[126] = '\x01';
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));
}

TEST(EDIDParserTest, ParseEDID) {
  uint16_t manufacturer_id = 0;
  uint16_t product_code = 0;
  std::string human_readable_name;
  std::vector<uint8_t> edid(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  gfx::Size pixel;
  gfx::Size size;
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    &human_readable_name, &pixel, &size));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ(0x6c28u, product_code);
  EXPECT_EQ("HP ZR30w", human_readable_name);
  EXPECT_EQ("2560x1600", pixel.ToString());
  EXPECT_EQ("641x400", size.ToString());

  manufacturer_id = 0;
  product_code = 0;
  human_readable_name.clear();
  Reset(&pixel, &size);
  edid.assign(kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));

  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    nullptr, &pixel, &size));
  EXPECT_EQ(0x4ca3u, manufacturer_id);
  EXPECT_EQ(0x4231u, product_code);
  EXPECT_EQ("", human_readable_name);
  EXPECT_EQ("1280x800", pixel.ToString());
  EXPECT_EQ("261x163", size.ToString());

  // Internal display doesn't have name.
  EXPECT_TRUE(ParseOutputDeviceData(edid, nullptr, nullptr,
                                    &human_readable_name, &pixel, &size));
  EXPECT_TRUE(human_readable_name.empty());

  manufacturer_id = 0;
  product_code = 0;
  human_readable_name.clear();
  Reset(&pixel, &size);
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    &human_readable_name, &pixel, &size));
  EXPECT_EQ(0x4c2du, manufacturer_id);
  EXPECT_EQ(0xfe08u, product_code);
  EXPECT_EQ("SAMSUNG", human_readable_name);
  EXPECT_EQ("1920x1080", pixel.ToString());
  EXPECT_EQ("160x90", size.ToString());
}

TEST(EDIDParserTest, ParseBrokenEDID) {
  uint16_t manufacturer_id = 0;
  uint16_t product_code = 0;
  std::string human_readable_name;
  std::vector<uint8_t> edid;

  gfx::Size dummy;

  // length == 0
  EXPECT_FALSE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                     &human_readable_name, &dummy, &dummy));

  // name is broken. Copying kNormalDisplay and substitute its name data by
  // some control code.
  edid.assign(kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));

  // display's name data is embedded in byte 95-107 in this specific example.
  // Fix here too when the contents of kNormalDisplay is altered.
  edid[97] = '\x1b';
  EXPECT_FALSE(ParseOutputDeviceData(edid, &manufacturer_id, nullptr,
                                     &human_readable_name, &dummy, &dummy));

  // If |human_readable_name| isn't specified, it skips parsing the name.
  manufacturer_id = 0;
  product_code = 0;
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    nullptr, &dummy, &dummy));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ(0x6c28u, product_code);
}

TEST(EDIDParserTest, GetDisplayId) {
  // EDID of kLP2565A and B are slightly different but actually the same device.
  int64_t id1 = -1;
  int64_t id2 = -1;
  int64_t product_id1 = -1;
  int64_t product_id2 = -1;
  std::vector<uint8_t> edid(kLP2565A, kLP2565A + charsize(kLP2565A));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id1, &product_id1));
  edid.assign(kLP2565B, kLP2565B + charsize(kLP2565B));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id2, &product_id2));
  EXPECT_EQ(id1, id2);
  // The product code in the two EDIDs varies.
  EXPECT_NE(product_id1, product_id2);
  EXPECT_EQ(0x22f07626, product_id1);
  EXPECT_EQ(0x22f07526, product_id2);
  EXPECT_NE(-1, id1);
  EXPECT_NE(-1, product_id1);
}

TEST(EDIDParserTest, GetDisplayIdFromInternal) {
  int64_t id = -1;
  int64_t product_id = -1;
  std::vector<uint8_t> edid(
      kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id, &product_id));
  EXPECT_NE(-1, id);
  EXPECT_NE(-1, product_id);
}

TEST(EDIDParserTest, GetDisplayIdFailure) {
  int64_t id = -1;
  int64_t product_id = -1;
  std::vector<uint8_t> edid;
  EXPECT_FALSE(GetDisplayIdFromEDID(edid, 0, &id, &product_id));
  EXPECT_EQ(-1, id);
  EXPECT_EQ(-1, product_id);
}

TEST(EDIDParserTest, ParseChromaticityCoordinates) {
  const std::vector<uint8_t> edid_normal_display(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  SkColorSpacePrimaries primaries_normal_display;
  EXPECT_TRUE(ParseChromaticityCoordinates(edid_normal_display,
                                           &primaries_normal_display));
  // Color primaries associated to |kNormalDisplay|; they don't correspond to
  // any standard color gamut.
  const SkColorSpacePrimaries kNormalDisplayPrimaries = {
      0.6777f, 0.3086f, 0.2080f, 0.6923f, 0.1465f, 0.0546f, 0.3125f, 0.3291f};
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, primaries_normal_display,
                      kNormalDisplayPrimaries);

  const std::vector<uint8_t> edid_internal_display(
      kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  SkColorSpacePrimaries primaries_internal_display;
  EXPECT_TRUE(ParseChromaticityCoordinates(edid_internal_display,
                                           &primaries_internal_display));
  // Color primaries associated to |kInternalDisplay|; they don't correspond to
  // any standard color gamut.
  const SkColorSpacePrimaries kInternalDisplayPrimaries = {
      0.5849f, 0.3603f, 0.3769f, 0.5654f, 0.1552f, 0.0996f, 0.3125f, 0.3291f};
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, primaries_internal_display,
                      kInternalDisplayPrimaries);

  const std::vector<uint8_t> edid_hpz32x(kHPz32x, kHPz32x + charsize(kHPz32x));
  SkColorSpacePrimaries primaries_hpz32x;
  EXPECT_TRUE(ParseChromaticityCoordinates(edid_hpz32x, &primaries_hpz32x));
  // Color primaries associated to |kHPz32x|; they don't correspond to
  // any standard color gamut.
  const SkColorSpacePrimaries kHPz32xPrimaries = {
      0.6738f, 0.3164f, 0.1962f, 0.7197f, 0.1484f, 0.0439f, 0.3144f, 0.3291f};
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, primaries_hpz32x,
                      kHPz32xPrimaries);

  const std::vector<uint8_t> edid_samus(kSamus, kSamus + charsize(kSamus));
  SkColorSpacePrimaries primaries_samus;
  EXPECT_TRUE(ParseChromaticityCoordinates(edid_samus, &primaries_samus));
  // Color primaries associated to Samus Chromebook, very similar to BT.709.
  const SkColorSpacePrimaries kSamusPrimaries = {
      0.6337f, 0.3476f, 0.3212f, 0.5771f, 0.1513f, 0.0908f, 0.3144f, 0.3291f};
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, primaries_samus,
                      kSamusPrimaries);

  const std::vector<uint8_t> edid_eve(kEve, kEve + charsize(kEve));
  SkColorSpacePrimaries primaries_eve;
  EXPECT_TRUE(ParseChromaticityCoordinates(edid_eve, &primaries_eve));
  // Color primaries associated to Eve Chromebook, very similar to BT.709.
  const SkColorSpacePrimaries kEvePrimaries = {
      0.6396f, 0.3291f, 0.2998f, 0.5996f, 0.1494f, 0.0595f, 0.3144f, 0.3281f};
  EXPECT_PRED_FORMAT2(SkColorSpacePrimariesEquals, primaries_eve,
                      kEvePrimaries);
}

TEST(EDIDParserTest, ParseGammaValue) {
  const std::vector<uint8_t> edid_normal_display(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  double edid_normal_display_gamma = 0.0;
  EXPECT_TRUE(ParseGammaValue(edid_normal_display, &edid_normal_display_gamma));
  EXPECT_DOUBLE_EQ(2.2, edid_normal_display_gamma);

  const std::vector<uint8_t> edid_internal_display(
      kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  double edid_internal_display_gamma = 0.0;
  EXPECT_TRUE(
      ParseGammaValue(edid_internal_display, &edid_internal_display_gamma));
  EXPECT_DOUBLE_EQ(2.2, edid_internal_display_gamma);

  const std::vector<uint8_t> edid_hpz32x(kHPz32x, kHPz32x + charsize(kHPz32x));
  double edid_hpz32x_gamma = 0.0;
  EXPECT_TRUE(ParseGammaValue(edid_hpz32x, &edid_hpz32x_gamma));
  EXPECT_DOUBLE_EQ(2.2, edid_hpz32x_gamma);

  const std::vector<uint8_t> edid_samus(kSamus, kSamus + charsize(kSamus));
  double edid_samus_gamma = 0.0;
  EXPECT_TRUE(ParseGammaValue(edid_samus, &edid_samus_gamma));
  EXPECT_DOUBLE_EQ(2.5, edid_samus_gamma);

  const std::vector<uint8_t> edid_eve(kEve, kEve + charsize(kEve));
  double edid_eve_gamma = 0.0;
  EXPECT_TRUE(ParseGammaValue(edid_eve, &edid_eve_gamma));
  EXPECT_DOUBLE_EQ(2.2, edid_eve_gamma);
}

TEST(EDIDParserTest, ParseBitsPerChannel) {
  const std::vector<uint8_t> edid_normal_display(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  int edid_normal_display_bits_per_channel = 0;
  EXPECT_TRUE(ParseBitsPerChannel(edid_normal_display,
                                  &edid_normal_display_bits_per_channel));
  EXPECT_EQ(10, edid_normal_display_bits_per_channel);

  const std::vector<uint8_t> edid_internal_display(
      kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  int edid_internal_display_bits_per_channel = 0;
  // |kInternalDisplay| doesn't have bits per channel information.
  EXPECT_FALSE(ParseBitsPerChannel(edid_internal_display,
                                   &edid_internal_display_bits_per_channel));

  const std::vector<uint8_t> edid_hpz32x(kHPz32x, kHPz32x + charsize(kHPz32x));
  int edid_hpz32x_bits_per_channel = 0;
  EXPECT_TRUE(ParseBitsPerChannel(edid_hpz32x, &edid_hpz32x_bits_per_channel));
  EXPECT_EQ(10, edid_hpz32x_bits_per_channel);

  const std::vector<uint8_t> edid_samus(kSamus, kSamus + charsize(kSamus));
  int edid_samus_bits_per_channel = 0;
  EXPECT_TRUE(ParseBitsPerChannel(edid_samus, &edid_samus_bits_per_channel));
  EXPECT_EQ(8, edid_samus_bits_per_channel);

  const std::vector<uint8_t> edid_eve(kEve, kEve + charsize(kEve));
  int edid_eve_bits_per_channel = 0;
  EXPECT_TRUE(ParseBitsPerChannel(edid_eve, &edid_eve_bits_per_channel));
  EXPECT_EQ(8, edid_eve_bits_per_channel);
}

}  // namespace display
