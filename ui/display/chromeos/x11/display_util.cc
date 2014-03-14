// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/display_util.h"

#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/x11/edid_parser_x11.h"

namespace ui {

namespace {

struct OutputTypeMapping {
  // Prefix of output name.
  std::string name;
  OutputType type;
};

const OutputTypeMapping kOutputTypeMapping[] = {
    {"LVDS", OUTPUT_TYPE_INTERNAL},
    {"eDP", OUTPUT_TYPE_INTERNAL},
    {"DSI", OUTPUT_TYPE_INTERNAL},
    {"VGA", OUTPUT_TYPE_VGA},
    {"HDMI", OUTPUT_TYPE_HDMI},
    {"DVI", OUTPUT_TYPE_DVI},
    {"DP", OUTPUT_TYPE_DISPLAYPORT}
};

// Gets some useful data from the specified output device, such like
// manufacturer's ID, product code, and human readable name. Returns false if it
// fails to get those data and doesn't touch manufacturer ID/product code/name.
// NULL can be passed for unwanted output parameters.
bool GetOutputDeviceData(XID output,
                         uint16* manufacturer_id,
                         std::string* human_readable_name) {
  unsigned long nitems = 0;
  unsigned char* prop = NULL;
  if (!base::GetEDIDProperty(output, &nitems, &prop))
    return false;

  bool result = base::ParseOutputDeviceData(
      prop, nitems, manufacturer_id, human_readable_name);
  XFree(prop);
  return result;
}

}  // namespace

OutputType GetOutputTypeFromName(const std::string& name) {
  for (unsigned int i = 0; i < arraysize(kOutputTypeMapping); ++i) {
    if (name.find(kOutputTypeMapping[i].name) == 0) {
      return kOutputTypeMapping[i].type;
    }
  }

  return OUTPUT_TYPE_UNKNOWN;
}

std::string GetDisplayName(const OutputConfigurator::OutputSnapshot& output) {
  std::string display_name;
  GetOutputDeviceData(output.output, NULL, &display_name);
  return display_name;
}

bool GetOutputOverscanFlag(const OutputConfigurator::OutputSnapshot& output,
                           bool* flag) {
  unsigned long nitems = 0;
  unsigned char* prop = NULL;
  if (!base::GetEDIDProperty(output.output, &nitems, &prop))
    return false;

  bool found = ParseOutputOverscanFlag(prop, nitems, flag);
  XFree(prop);
  return found;
}

bool ParseOutputOverscanFlag(const unsigned char* prop,
                             unsigned long nitems,
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

  if (nitems <= kNumExtensionsOffset)
    return false;

  unsigned char num_extensions = prop[kNumExtensionsOffset];

  for (size_t i = 0; i < num_extensions; ++i) {
    // Skip parsing the whole extension if size is not enough.
    if (nitems < kExtensionBase + (i + 1) * kExtensionSize)
      break;

    const unsigned char* extension = prop + kExtensionBase + i * kExtensionSize;
    unsigned char tag = extension[0];
    unsigned char revision = extension[1];
    if (tag != kCEAExtensionTag || revision != kExpectedExtensionRevision)
      continue;

    unsigned char timing_descriptors_start =
        std::min(extension[2], static_cast<unsigned char>(kExtensionSize));
    const unsigned char* data_block = extension + kDataBlockOffset;
    while (data_block < extension + timing_descriptors_start) {
      // A data block is encoded as:
      // - byte 1 high 3 bits: tag. '07' for extended tags.
      // - byte 1 remaining bits: the length of data block.
      // - byte 2: the extended tag.  '0' for video capability.
      // - byte 3: the capability.
      unsigned char tag = data_block[0] >> 5;
      unsigned char payload_length = data_block[0] & 0x1f;
      if (static_cast<unsigned long>(data_block + payload_length - prop) >
          nitems)
        break;

      if (tag != kExtendedTag || payload_length < 2) {
        data_block += payload_length + 1;
        continue;
      }

      unsigned char extended_tag_code = data_block[1];
      if (extended_tag_code != kExtendedVideoCapabilityTag) {
        data_block += payload_length + 1;
        continue;
      }

      // The difference between preferred, IT, and CE video formats
      // doesn't matter. Sets |flag| to true if any of these flags are true.
      if ((data_block[2] & (1 << kPTOverscan)) ||
          (data_block[2] & (1 << kITOverscan)) ||
          (data_block[2] & (1 << kCEOverscan))) {
        *flag = true;
      } else {
        *flag = false;
      }
      return true;
    }
  }

  return false;
}

}  // namespace ui
