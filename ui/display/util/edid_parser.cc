// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stddef.h>

#include <algorithm>

#include "base/hash.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display {

namespace {

// Returns a 32-bit identifier for this model of display, using
// |manufacturer_id| and |product_code|.
uint32_t GetProductID(uint16_t manufacturer_id, uint16_t product_code) {
  return ((static_cast<uint32_t>(manufacturer_id) << 16) |
          (static_cast<uint32_t>(product_code)));
}

}  // namespace

bool GetDisplayIdFromEDID(const std::vector<uint8_t>& edid,
                          uint8_t output_index,
                          int64_t* display_id_out,
                          int64_t* product_id_out) {
  uint16_t manufacturer_id = 0;
  uint16_t product_code = 0;
  std::string product_name;

  // ParseOutputDeviceData fails if it doesn't have product_name.
  ParseOutputDeviceData(edid, &manufacturer_id, &product_code, &product_name,
                        nullptr, nullptr);

  if (manufacturer_id == 0)
    return false;

  // Generates product specific value from product_name instead of product code.
  // See crbug.com/240341
  uint32_t product_code_hash =
      product_name.empty() ? 0 : base::Hash(product_name);
  // An ID based on display's index will be assigned later if this call fails.
  *display_id_out =
      GenerateDisplayID(manufacturer_id, product_code_hash, output_index);
  // |product_id_out| is 64-bit signed so it can store -1 as kInvalidProductID
  // and not match a valid product id which will all be in the lowest 32-bits.
  if (product_id_out)
    *product_id_out = GetProductID(manufacturer_id, product_code);
  return true;
}

bool ParseOutputDeviceData(const std::vector<uint8_t>& edid,
                           uint16_t* manufacturer_id,
                           uint16_t* product_code,
                           std::string* human_readable_name,
                           gfx::Size* active_pixel_out,
                           gfx::Size* physical_display_size_out) {
  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the details of EDID data format.  We use the following data:
  //   bytes 8-9: manufacturer EISA ID, in big-endian
  //   bytes 10-11: manufacturer product code, in little-endian
  //   bytes 54-125: four descriptors (18-bytes each) which may contain
  //     the display name.
  const unsigned int kManufacturerOffset = 8;
  const unsigned int kManufacturerLength = 2;
  const unsigned int kProductCodeOffset = 10;
  const unsigned int kProductCodeLength = 2;
  const unsigned int kDescriptorOffset = 54;
  const unsigned int kNumDescriptors = 4;
  const unsigned int kDescriptorLength = 18;
  // The specifier types.
  const unsigned char kMonitorNameDescriptor = 0xfc;

  if (manufacturer_id) {
    if (edid.size() < kManufacturerOffset + kManufacturerLength) {
      LOG(ERROR) << "too short EDID data: manufacturer id";
      return false;
    }

    // ICC filename is generated based on these ids. We always read this as big
    // endian so that the file name matches bytes 8-11 as they appear in EDID.
    *manufacturer_id =
        (edid[kManufacturerOffset] << 8) + edid[kManufacturerOffset + 1];
  }

  if (product_code) {
    if (edid.size() < kProductCodeOffset + kProductCodeLength) {
      LOG(ERROR) << "too short EDID data: manufacturer product code";
      return false;
    }

    *product_code =
        (edid[kProductCodeOffset] << 8) + edid[kProductCodeOffset + 1];
  }

  if (human_readable_name)
    human_readable_name->clear();

  for (unsigned int i = 0; i < kNumDescriptors; ++i) {
    if (edid.size() < kDescriptorOffset + (i + 1) * kDescriptorLength)
      break;

    size_t offset = kDescriptorOffset + i * kDescriptorLength;

    // Detailed Timing Descriptor:
    if (edid[offset] != 0 && edid[offset + 1] != 0) {
      const int kMaxResolution = 10080;  // 8k display.

      if (active_pixel_out) {
        const int kHorizontalPixelLsbOffset = 2;
        const int kHorizontalPixelMsbOffset = 4;
        const int kVerticalPixelLsbOffset = 5;
        const int kVerticalPixelMsbOffset = 7;

        int h_lsb = edid[offset + kHorizontalPixelLsbOffset];
        int h_msb = edid[offset + kHorizontalPixelMsbOffset];
        int h_pixel = std::min(h_lsb + ((h_msb & 0xF0) << 4), kMaxResolution);

        int v_lsb = edid[offset + kVerticalPixelLsbOffset];
        int v_msb = edid[offset + kVerticalPixelMsbOffset];
        int v_pixel = std::min(v_lsb + ((v_msb & 0xF0) << 4), kMaxResolution);

        active_pixel_out->SetSize(h_pixel, v_pixel);
        // EDID may contain multiple DTD. Use first one that
        // contains the highest resolution.
        active_pixel_out = nullptr;
      }

      if (physical_display_size_out) {
        const int kHorizontalSizeLsbOffset = 12;
        const int kVerticalSizeLsbOffset = 13;
        const int kSizeMsbOffset = 14;

        int h_lsb = edid[offset + kHorizontalSizeLsbOffset];
        int v_lsb = edid[offset + kVerticalSizeLsbOffset];

        int msb = edid[offset + kSizeMsbOffset];
        int h_size = h_lsb + ((msb & 0xF0) << 4);
        int v_size = v_lsb + ((msb & 0x0F) << 8);
        physical_display_size_out->SetSize(h_size, v_size);
        physical_display_size_out = nullptr;
      }
      continue;
    }

    // EDID Other Monitor Descriptors:
    // If the descriptor contains the display name, it has the following
    // structure:
    //   bytes 0-2, 4: \0
    //   byte 3: descriptor type, defined above.
    //   bytes 5-17: text data, ending with \r, padding with spaces
    // we should check bytes 0-2 and 4, since it may have other values in
    // case that the descriptor contains other type of data.
    if (edid[offset] == 0 && edid[offset + 1] == 0 && edid[offset + 2] == 0 &&
        edid[offset + 3] == kMonitorNameDescriptor && edid[offset + 4] == 0 &&
        human_readable_name) {
      std::string found_name(reinterpret_cast<const char*>(&edid[offset + 5]),
                             kDescriptorLength - 5);
      base::TrimWhitespaceASCII(
          found_name, base::TRIM_TRAILING, human_readable_name);
      continue;
    }
  }

  // Verify if the |human_readable_name| consists of printable characters only.
  // TODO(oshima|muka): Consider replacing unprintable chars with white space.
  if (human_readable_name) {
    for (size_t i = 0; i < human_readable_name->size(); ++i) {
      char c = (*human_readable_name)[i];
      if (!isascii(c) || !isprint(c)) {
        human_readable_name->clear();
        LOG(ERROR) << "invalid EDID: human unreadable char in name";
        return false;
      }
    }
  }

  return true;
}

bool ParseOutputOverscanFlag(const std::vector<uint8_t>& edid,
                             bool* flag) {
  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the extension format of EDID.  Also see EIA/CEA-861 spec for
  // the format of the extensions and how video capability is encoded.
  //  - byte 0: tag.  should be 02h.
  //  - byte 1: revision.  only cares revision 3 (03h).
  //  - byte 4-: data block.
  const unsigned int kExtensionBase = 128;
  const unsigned int kExtensionSize = 128;
  const unsigned int kNumExtensionsOffset = 126;
  const unsigned int kDataBlockOffset = 4;
  const unsigned char kCEAExtensionTag = '\x02';
  const unsigned char kExpectedExtensionRevision = '\x03';
  const unsigned char kExtendedTag = 7;
  const unsigned char kExtendedVideoCapabilityTag = 0;
  const unsigned int kPTOverscan = 4;
  const unsigned int kITOverscan = 2;
  const unsigned int kCEOverscan = 0;

  if (edid.size() <= kNumExtensionsOffset)
    return false;

  unsigned char num_extensions = edid[kNumExtensionsOffset];

  for (size_t i = 0; i < num_extensions; ++i) {
    // Skip parsing the whole extension if size is not enough.
    if (edid.size() < kExtensionBase + (i + 1) * kExtensionSize)
      break;

    size_t extension_offset = kExtensionBase + i * kExtensionSize;
    unsigned char tag = edid[extension_offset];
    unsigned char revision = edid[extension_offset + 1];
    if (tag != kCEAExtensionTag || revision != kExpectedExtensionRevision)
      continue;

    unsigned char timing_descriptors_start = std::min(
        edid[extension_offset + 2], static_cast<unsigned char>(kExtensionSize));

    for (size_t data_offset = extension_offset + kDataBlockOffset;
         data_offset < extension_offset + timing_descriptors_start;) {
      // A data block is encoded as:
      // - byte 1 high 3 bits: tag. '07' for extended tags.
      // - byte 1 remaining bits: the length of data block.
      // - byte 2: the extended tag.  '0' for video capability.
      // - byte 3: the capability.
      unsigned char tag = edid[data_offset] >> 5;
      unsigned char payload_length = edid[data_offset] & 0x1f;
      if (data_offset + payload_length + 1 > edid.size())
        break;

      if (tag != kExtendedTag || payload_length < 2 ||
          edid[data_offset + 1] != kExtendedVideoCapabilityTag) {
        data_offset += payload_length + 1;
        continue;
      }

      // The difference between preferred, IT, and CE video formats
      // doesn't matter. Sets |flag| to true if any of these flags are true.
      if ((edid[data_offset + 2] & (1 << kPTOverscan)) ||
          (edid[data_offset + 2] & (1 << kITOverscan)) ||
          (edid[data_offset + 2] & (1 << kCEOverscan))) {
        *flag = true;
      } else {
        *flag = false;
      }
      return true;
    }
  }

  return false;
}

bool ParseChromaticityCoordinates(const std::vector<uint8_t>& edid,
                                  SkColorSpacePrimaries* primaries) {
  DCHECK(primaries);

  // Offsets, lengths, positions and masks are taken from [1] (or [2]).
  // [1] http://en.wikipedia.org/wiki/Extended_display_identification_data
  // [2] "VESA Enhanced EDID Standard " Release A, Revision 1, Feb 2000, Sec 3.7
  //  "Phosphor or Filter Chromaticity: 10 bytes"
  const unsigned int kChromaticityOffset = 25;
  const unsigned int kChromaticityLength = 10;

  const unsigned int kRedGreenLsbOffset = 25;
  const unsigned int kRedxLsbPosition = 6;
  const unsigned int kRedyLsbPosition = 4;
  const unsigned int kGreenxLsbPosition = 3;
  const unsigned int kGreenyLsbPosition = 0;

  const unsigned int kBlueWhiteLsbOffset = 26;
  const unsigned int kBluexLsbPosition = 6;
  const unsigned int kBlueyLsbPosition = 4;
  const unsigned int kWhitexLsbPosition = 3;
  const unsigned int kWhiteyLsbPosition = 0;

  // All LSBits parts are 2 bits wide.
  const unsigned int kLsbMask = 0x3;

  const unsigned int kRedxMsbOffset = 27;
  const unsigned int kRedyMsbOffset = 28;
  const unsigned int kGreenxMsbOffset = 29;
  const unsigned int kGreenyMsbOffset = 30;
  const unsigned int kBluexMsbOffset = 31;
  const unsigned int kBlueyMsbOffset = 32;
  const unsigned int kWhitexMsbOffset = 33;
  const unsigned int kWhiteyMsbOffset = 34;

  static_assert(
      kChromaticityOffset + kChromaticityLength == kWhiteyMsbOffset + 1,
      "EDID Parameter section length error");

  if (edid.size() < kChromaticityOffset + kChromaticityLength) {
    LOG(ERROR) << "too short EDID data: chromaticity coordinates";
    return false;
  }

  const uint8_t red_green_lsbs = edid[kRedGreenLsbOffset];
  const uint8_t blue_white_lsbs = edid[kBlueWhiteLsbOffset];

  // Recompose the 10b values by appropriately mixing the 8 MSBs and the 2 LSBs,
  // then rescale to 1024;
  primaries->fRX = ((edid[kRedxMsbOffset] << 2) +
                    ((red_green_lsbs >> kRedxLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fRY = ((edid[kRedyMsbOffset] << 2) +
                    ((red_green_lsbs >> kRedyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fGX = ((edid[kGreenxMsbOffset] << 2) +
                    ((red_green_lsbs >> kGreenxLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fGY = ((edid[kGreenyMsbOffset] << 2) +
                    ((red_green_lsbs >> kGreenyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fBX = ((edid[kBluexMsbOffset] << 2) +
                    ((blue_white_lsbs >> kBluexLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fBY = ((edid[kBlueyMsbOffset] << 2) +
                    ((blue_white_lsbs >> kBlueyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fWX = ((edid[kWhitexMsbOffset] << 2) +
                    ((blue_white_lsbs >> kWhitexLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries->fWY = ((edid[kWhiteyMsbOffset] << 2) +
                    ((blue_white_lsbs >> kWhiteyLsbPosition) & kLsbMask)) /
                   1024.0f;

  // TODO(mcasas): Up to two additional White Point coordinates can be provided
  // in a Display Descriptor.Read them if we are not satisfied with |fWX| or
  // |FWy|. https://crbug.com/771345.
  return true;
}

}  // namespace display
