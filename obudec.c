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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./obudec.h"

#include "aom_ports/mem_ops.h"
#include "av1/common/common.h"
#include "av1/decoder/obu.h"

#define OBU_BUFFER_SIZE (500 * 1024)

#define OBU_HEADER_SIZE 1
#define OBU_EXTENSION_SIZE 1
#define OBU_MAX_LENGTH_FIELD_SIZE 8
#define OBU_DETECTION_SIZE \
  (OBU_HEADER_SIZE + OBU_EXTENSION_SIZE + OBU_MAX_LENGTH_FIELD_SIZE)

// Reads unsigned LEB128 integer and returns 0 upon successful read and decode.
// Stores raw bytes in 'value_buffer', length of the number in 'value_length',
// and decoded value in 'value'.
static int obudec_read_leb128(FILE *f, uint8_t *value_buffer,
                              size_t *value_length, uint64_t *value) {
  if (!f || !value_buffer || !value_length || !value) return -1;
  for (int len = 0; len < OBU_MAX_LENGTH_FIELD_SIZE; ++len) {
    const size_t num_read = fread(&value_buffer[len], 1, 1, f);
    if (num_read == 0 && feof(f)) {
      *value_length = 0;
      return 0;
    }
    if (num_read != 1) {
      // Ran out of data before completing read of value.
      return -1;
    }
    if ((value_buffer[len] >> 7) == 0) {
      *value_length = (size_t)(len + 1);
      break;
    }
  }

  return aom_uleb_decode(value_buffer, OBU_MAX_LENGTH_FIELD_SIZE, value, NULL);
}

// Reads OBU size from infile and returns 0 upon success. Returns obu_size via
// output pointer obu_size. Returns -1 when reading or parsing fails. Always
// returns FILE pointer to position at time of call. Returns 0 and sets obu_size
// to 0 when end of file is reached.
static int obudec_read_obu_size(FILE *infile, uint64_t *obu_size,
                                size_t *length_field_size) {
  if (!infile || !obu_size) return 1;

  uint8_t read_buffer[OBU_MAX_LENGTH_FIELD_SIZE] = { 0 };
  size_t bytes_read = fread(read_buffer, 1, OBU_MAX_LENGTH_FIELD_SIZE, infile);
  *obu_size = 0;

  if (bytes_read == 0) {
    return 0;
  }

  const int seek_pos = (int)bytes_read;
  if (seek_pos != 0 && fseek(infile, -seek_pos, SEEK_CUR) != 0) return 1;

  if (aom_uleb_decode(read_buffer, bytes_read, obu_size, length_field_size) !=
      0) {
    return 1;
  }

  return 0;
}

// Reads OBU header from 'f'. The 'buffer_capacity' passed in must be large
// enough to store an OBU header with extension (2 bytes). Raw OBU data is
// written to 'obu_data', parsed OBU header values are written to 'obu_header',
// and total bytes read from file are written to 'bytes_read'. Returns 0 for
// success, and non-zero on failure. When end of file is reached, the return
// value is 0 and the 'bytes_read' value is set to 0.
static int obudec_read_obu_header(FILE *f, size_t buffer_capacity,
                                  int is_annexb, uint8_t *obu_data,
                                  ObuHeader *obu_header, size_t *bytes_read) {
  if (!f || buffer_capacity < (OBU_HEADER_SIZE + OBU_EXTENSION_SIZE) ||
      !obu_data || !obu_header || !bytes_read) {
    return -1;
  }
  *bytes_read = fread(obu_data, 1, 1, f);

  if (feof(f) && *bytes_read == 0) {
    return 0;
  } else if (*bytes_read != 1) {
    fprintf(stderr, "obudec: Failure reading OBU header.\n");
    return -1;
  }

  const int has_extension = (obu_data[0] >> 2) & 0x1;
  if (has_extension) {
    if (fread(&obu_data[1], 1, 1, f) != 1) {
      fprintf(stderr, "obudec: Failure reading OBU extension.");
      return -1;
    }
    ++*bytes_read;
  }

  size_t obu_bytes_parsed = 0;
  const aom_codec_err_t parse_result = aom_read_obu_header(
      obu_data, *bytes_read, &obu_bytes_parsed, obu_header, is_annexb);
  if (parse_result != AOM_CODEC_OK || *bytes_read != obu_bytes_parsed) {
    fprintf(stderr, "obudec: Error parsing OBU header.\n");
    return -1;
  }

  return 0;
}

// Reads OBU payload from 'f' and returns 0 for success when all payload bytes
// are read from the file. Payload data is written to 'obu_data', and actual
// bytes read written to 'bytes_read'.
static int obudec_read_obu_payload(FILE *f, uint64_t payload_length,
                                   uint8_t *obu_data, size_t *bytes_read) {
  if (!f || payload_length == 0 || !obu_data || !bytes_read) return -1;

  if (fread(obu_data, 1, (size_t)payload_length, f) != payload_length) {
    fprintf(stderr, "obudec: Failure reading OBU payload.\n");
    return -1;
  }

  *bytes_read += (size_t)payload_length;
  return 0;
}

static int obudec_read_one_obu(FILE *f, size_t buffer_capacity, int is_annexb,
                               uint8_t *obu_data, uint64_t *obu_length,
                               ObuHeader *obu_header) {
  const size_t kMinimumBufferSize = OBU_DETECTION_SIZE;
  if (!f || !obu_data || !obu_length || !obu_header ||
      buffer_capacity < kMinimumBufferSize) {
    return -1;
  }

  uint64_t obu_payload_length = 0;
  size_t leb128_length = 0;
  uint64_t obu_size = 0;
  if (is_annexb) {
    if (obudec_read_leb128(f, &obu_data[0], &leb128_length, &obu_size) != 0) {
      fprintf(stderr, "obudec: Failure reading OBU size length.\n");
      return -1;
    } else if (leb128_length == 0) {
      *obu_length = 0;
      return 0;
    }
  }

  size_t bytes_read = 0;
  if (obudec_read_obu_header(f, buffer_capacity, is_annexb,
                             obu_data + leb128_length, obu_header,
                             &bytes_read) != 0) {
    return -1;
  } else if (bytes_read == 0) {
    *obu_length = 0;
    return 0;
  }

  if (!is_annexb) {
    if (obudec_read_leb128(f, &obu_data[bytes_read], &leb128_length,
                           &obu_payload_length) != 0) {
      fprintf(stderr, "obudec: Failure reading OBU payload length.\n");
      return -1;
    }
  }

  if (is_annexb) {
    obu_payload_length = obu_size - bytes_read;
  }

  bytes_read += leb128_length;

  if (UINT64_MAX - bytes_read < obu_payload_length) return -1;
  if (bytes_read + obu_payload_length > buffer_capacity) {
    *obu_length = bytes_read + obu_payload_length;
    return -1;
  }

  if (obu_payload_length > 0 &&
      obudec_read_obu_payload(f, obu_payload_length, &obu_data[bytes_read],
                              &bytes_read) != 0) {
    return -1;
  }

  *obu_length = bytes_read;
  return 0;
}

int file_is_obu(struct ObuDecInputContext *obu_ctx) {
  if (!obu_ctx || !obu_ctx->avx_ctx) return 0;

  struct AvxInputContext *avx_ctx = obu_ctx->avx_ctx;
  uint8_t detect_buf[OBU_DETECTION_SIZE] = { 0 };
  const int is_annexb = obu_ctx->is_annexb;
  FILE *f = avx_ctx->file;
  uint64_t obu_length = 0;
  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));

  if (obudec_read_one_obu(f, OBU_DETECTION_SIZE, is_annexb, &detect_buf[0],
                          &obu_length, &obu_header) != 0) {
    fprintf(stderr, "obudec: Failure reading first OBU.\n");
    rewind(f);
    return 0;
  }

  if (obu_header.type != OBU_TEMPORAL_DELIMITER) return 0;

  if (obu_header.has_length_field) {
    uint64_t obu_payload_length = 0;
    size_t leb128_length = 0;
    const size_t obu_length_offset = obu_header.has_length_field ? 1 : 2;
    if (aom_uleb_decode(&detect_buf[obu_length_offset], sizeof(leb128_length),
                        &obu_payload_length, &leb128_length) != 0) {
      fprintf(stderr, "obudec: Failure decoding OBU payload length.\n");
      rewind(f);
      return 0;
    }
    if (obu_payload_length != 0) {
      fprintf(
          stderr,
          "obudec: Invalid OBU_TEMPORAL_DELIMITER payload length (non-zero).");
      rewind(f);
      return 0;
    }
  } else if (!is_annexb) {
    fprintf(stderr, "obudec: OBU size fields required, cannot decode input.\n");
    rewind(f);
    return (0);
  }

  // Appears that input is valid Section 5 AV1 stream.
  obu_ctx->buffer = (uint8_t *)calloc(OBU_BUFFER_SIZE, 1);
  if (!obu_ctx->buffer) {
    fprintf(stderr, "Out of memory.\n");
    rewind(f);
    return 0;
  }
  obu_ctx->buffer_capacity = OBU_BUFFER_SIZE;
  memcpy(obu_ctx->buffer, &detect_buf[0], (size_t)obu_length);
  obu_ctx->bytes_buffered = (size_t)obu_length;

  return 1;
}

int obudec_read_temporal_unit(struct ObuDecInputContext *obu_ctx,
                              uint8_t **buffer, size_t *bytes_read,
                              size_t *buffer_size) {
  FILE *f = obu_ctx->avx_ctx->file;
  if (!f) return -1;

  *buffer_size = 0;
  *bytes_read = 0;

  if (feof(f)) {
    return 1;
  }

  const int is_annexb = obu_ctx->is_annexb;
  while (1) {
    ObuHeader obu_header;
    memset(&obu_header, 0, sizeof(obu_header));

    uint64_t obu_size = 0;
    uint8_t *data = obu_ctx->buffer + obu_ctx->bytes_buffered;
    const size_t capacity = obu_ctx->buffer_capacity - obu_ctx->bytes_buffered;

    if (obudec_read_one_obu(f, capacity, is_annexb, data, &obu_size,
                            &obu_header) != 0) {
      fprintf(stderr, "obudec: read_one_obu failed in TU loop\n");
      return -1;
    }

    if (obu_header.type == OBU_TEMPORAL_DELIMITER || obu_size == 0 ||
        (obu_header.has_extension &&
         obu_header.enhancement_layer_id > obu_ctx->last_layer_id)) {
      const size_t tu_size = obu_ctx->bytes_buffered;

#if defined AOM_MAX_ALLOCABLE_MEMORY
      if (tu_size > AOM_MAX_ALLOCABLE_MEMORY) {
        fprintf(stderr, "obudec: Temporal Unit size exceeds max alloc size.\n");
        return -1;
      }
#endif
      uint8_t *new_buffer = (uint8_t *)realloc(*buffer, tu_size);
      if (!new_buffer) {
        free(*buffer);
        fprintf(stderr, "obudec: Out of memory.\n");
        return -1;
      }
      *buffer = new_buffer;
      *bytes_read = tu_size;
      *buffer_size = tu_size;
      memcpy(*buffer, obu_ctx->buffer, tu_size);

      memmove(obu_ctx->buffer, data, (size_t)obu_size);
      obu_ctx->bytes_buffered = (size_t)obu_size;
      break;
    } else {
      obu_ctx->bytes_buffered += (size_t)obu_size;
    }
  }

  return 0;
}

void obudec_free(struct ObuDecInputContext *obu_ctx) { free(obu_ctx->buffer); }
