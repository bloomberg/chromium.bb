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

#ifndef TOOLS_OBU_PARSER_H_
#define TOOLS_OBU_PARSER_H_

#include <cstdint>

namespace aom_tools {

// TODO(tomfinegan): The OBU and OBU metadata types are currently defined in
// av1/common/enums.h, which is not part of the libaom's public API. They are
// duped here until that's dealt with.

enum ObuType {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_PADDING = 15,
};

enum ObuMetadataType {
  OBU_METADATA_TYPE_PRIVATE_DATA = 0,
  OBU_METADATA_TYPE_HDR_CLL = 1,
  OBU_METADATA_TYPE_HDR_MDCV = 2,
};

struct ObuExtensionHeader {
  int temporal_id;
  int spatial_id;
  int quality_id;
  bool reserved_flag;
};

struct ObuHeader {
  int type;
  int reserved;
  bool has_extension;
  ObuExtensionHeader ext_header;
};

// Print information obtained from OBU(s) in data until data is exhausted or an
// error occurs. Returns true when all data is consumed successfully, and
// optionally reports OBU storage overhead via obu_overhead_bytes when the
// pointer is non-null.
bool DumpObu(const uint8_t *data, int length, int *obu_overhead_bytes);

}  // namespace aom_tools

#endif  // TOOLS_OBU_PARSER_H_
