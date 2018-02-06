/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdio>
#include <string>

// TODO(tomfinegan): Remove the aom_config.h include as soon as possible. At
// present it's required because aom_config.h determines if the library writes
// OBUs.
#include "./aom_config.h"

#if !CONFIG_OBU
#error "obu_parser.cc requires CONFIG_OBU"
#endif

#include "aom/aom_integer.h"
#include "aom_ports/mem_ops.h"
#include "tools/obu_parser.h"

namespace {
const aom_tools::ObuExtensionHeader kEmptyObuExt = { 0, 0, 0, false };
const aom_tools::ObuHeader kEmptyObu = { 0, 0, false, kEmptyObuExt };
}  // namespace

namespace aom_tools {

// Basic OBU syntax
// 4 bytes: length
// 8 bits: Header
//   7
//     forbidden bit
//   6,5,4,3
//     type bits
//   2,1
//     reserved bits
//   0
//     extension bit
const uint32_t kObuForbiddenBitMask = 0x1;
const uint32_t kObuForbiddenBitShift = 7;
const uint32_t kObuTypeBitsMask = 0xF;
const uint32_t kObuTypeBitsShift = 3;
const uint32_t kObuReservedBitsMask = 0x3;
const uint32_t kObuReservedBitsShift = 1;
const uint32_t kObuExtensionFlagBitMask = 0x1;

// When extension bit is set:
// 8 bits: extension header
// 7,6,5
//   temporal ID
// 4,3
//   spatial ID
// 2,1
//   quality ID
// 0
//   reserved bit
const uint32_t kObuExtTemporalIdBitsMask = 0x7;
const uint32_t kObuExtTemporalIdBitsShift = 5;
const uint32_t kObuExtSpatialIdBitsMask = 0x3;
const uint32_t kObuExtSpatialIdBitsShift = 3;
const uint32_t kObuExtQualityIdBitsMask = 0x3;
const uint32_t kObuExtQualityIdBitsShift = 1;
const uint32_t kObuExtReservedFlagBit = 0x1;

bool ValidObuType(int obu_type) {
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_METADATA:
    case OBU_PADDING: return true;
  }
  return false;
}

bool ParseObuHeader(uint8_t obu_header_byte, ObuHeader *obu_header) {
  const int forbidden_bit =
      (obu_header_byte >> kObuForbiddenBitShift) & kObuForbiddenBitMask;
  if (forbidden_bit) {
    fprintf(stderr, "Invalid OBU, forbidden bit set.\n");
    return false;
  }

  obu_header->type = (obu_header_byte >> kObuTypeBitsShift) & kObuTypeBitsMask;
  if (!ValidObuType(obu_header->type)) {
    fprintf(stderr, "Invalid OBU type: %d.\n", obu_header->type);
    return false;
  }

  obu_header->reserved =
      (obu_header_byte >> kObuReservedBitsShift) & kObuReservedBitsMask;
  if (obu_header->reserved != 0) {
    fprintf(stderr, "Invalid OBU: reserved bit(s) set.\n");
    return false;
  }

  obu_header->has_extension = obu_header_byte & kObuExtensionFlagBitMask;
  return true;
}

bool ParseObuExtensionHeader(uint8_t ext_header_byte,
                             ObuExtensionHeader *ext_header) {
  ext_header->temporal_id = (ext_header_byte >> kObuExtTemporalIdBitsShift) &
                            kObuExtTemporalIdBitsMask;
  ext_header->spatial_id =
      (ext_header_byte >> kObuExtSpatialIdBitsShift) & kObuExtSpatialIdBitsMask;
  ext_header->quality_id =
      (ext_header_byte >> kObuExtQualityIdBitsShift) & kObuExtQualityIdBitsMask;
  ext_header->reserved_flag = ext_header_byte & kObuExtReservedFlagBit;

  if (ext_header->reserved_flag) {
    fprintf(stderr, "Invalid OBU Extension: reserved flag set.\n");
    return false;
  }

  return true;
}

std::string ObuTypeToString(int obu_type) {
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER: return "OBU_SEQUENCE_HEADER";
    case OBU_TEMPORAL_DELIMITER: return "OBU_TEMPORAL_DELIMITER";
    case OBU_FRAME_HEADER: return "OBU_FRAME_HEADER";
    case OBU_TILE_GROUP: return "OBU_TILE_GROUP";
    case OBU_METADATA: return "OBU_METADATA";
    case OBU_PADDING: return "OBU_PADDING";
  }
  return "";
}

void PrintObuHeader(const ObuHeader *header) {
  printf(
      "  OBU type:        %s\n"
      "      extension:   %s\n",
      ObuTypeToString(header->type).c_str(),
      header->has_extension ? "yes" : "no");
  if (header->has_extension) {
    printf(
        "      temporal_id: %d\n"
        "      spatial_id:  %d\n"
        "      quality_id:  %d\n",
        header->ext_header.temporal_id, header->ext_header.spatial_id,
        header->ext_header.quality_id);
  }
}

bool DumpObu(const uint8_t *data, int length) {
  const int kObuHeaderLengthSizeBytes = 1;
  const int kMinimumBytesRequired = 1 + kObuHeaderLengthSizeBytes;
  int consumed = 0;
  ObuHeader obu_header;
  while (consumed < length) {
    const int remaining = length - consumed;
    if (remaining < kMinimumBytesRequired) {
      if (remaining > 0) {
        fprintf(stderr,
                "OBU parse error. Did not consume all data, %d bytes "
                "remain.\n",
                remaining);
      }
      return false;
    }

#if CONFIG_OBU_SIZING
    uint64_t obu_size = 0;
    aom_uleb_decode(data + consumed, remaining, &obu_size);
    const int current_obu_length = static_cast<int>(obu_size);
    const size_t length_field_size = aom_uleb_size_in_bytes(obu_size);
#else
    const int current_obu_length = mem_get_le32(data + consumed);
    const int kObuLengthFieldSizeBytes = 4;
    const size_t length_field_size = kObuLengthFieldSizeBytes;
#endif  // CONFIG_OBU_SIZING

    if (current_obu_length > remaining) {
      fprintf(stderr,
              "OBU parsing failed at offset %d with bad length of %d "
              "and %d bytes left.\n",
              consumed, current_obu_length, remaining);
      return false;
    }
    consumed += length_field_size;

    obu_header = kEmptyObu;
    const uint8_t obu_header_byte = *(data + consumed);
    if (!ParseObuHeader(obu_header_byte, &obu_header)) {
      fprintf(stderr, "OBU parsing failed at offset %d.\n", consumed);
      return false;
    }

    if (obu_header.has_extension) {
      const uint8_t obu_ext_header_byte =
          *(data + consumed + kObuHeaderLengthSizeBytes);
      if (!ParseObuExtensionHeader(obu_ext_header_byte,
                                   &obu_header.ext_header)) {
        fprintf(stderr, "OBU extension parsing failed at offset %d.\n",
                consumed);
        return false;
      }
    }

    PrintObuHeader(&obu_header);

    // TODO(tomfinegan): Parse OBU payload. For now just consume it.
    consumed += current_obu_length;
  }

  return true;
}

}  // namespace aom_tools
