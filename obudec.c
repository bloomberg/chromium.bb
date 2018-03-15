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

#define OBU_HEADER_SIZE_BYTES 1
#define OBU_HEADER_EXTENSION_SIZE_BYTES 1

// Unsigned LEB128 OBU length field has maximum size of 8 bytes.
#define OBU_MAX_LENGTH_FIELD_SIZE 8
#define OBU_MAX_HEADER_SIZE                                  \
  (OBU_HEADER_SIZE_BYTES + OBU_HEADER_EXTENSION_SIZE_BYTES + \
   OBU_MAX_LENGTH_FIELD_SIZE)

#if CONFIG_OBU_NO_IVF

// Reads OBU size from infile and returns 0 upon success. Returns obu_size via
// output pointer obu_size. Returns -1 when reading or parsing fails. Always
// returns FILE pointer to position at time of call. Returns 0 and sets obu_size
// to 0 when end of file is reached.
int read_obu_size(FILE *infile, uint64_t *obu_size, size_t *length_field_size) {
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

int obu_read_temporal_unit(FILE *infile, uint8_t **buffer, size_t *bytes_read,
#if CONFIG_SCALABILITY
                           size_t *buffer_size, int last_layer_id) {
#else
                           size_t *buffer_size) {
#endif
  size_t ret;
  uint64_t obu_size = 0;
  uint8_t *data = NULL;

  if (feof(infile)) {
    return 1;
  }

  *buffer_size = 0;
  *bytes_read = 0;
  while (1) {
    size_t length_field_size = 0;
    ret = read_obu_size(infile, &obu_size, &length_field_size);
    if (ret != 0) {
      warn("Failed to read OBU size.\n");
      return 1;
    }
    if (ret == 0 && obu_size == 0) {
      fprintf(stderr, "Found end of stream, ending temporal unit\n");
      break;
    }

    obu_size += length_field_size;

    // Expand the buffer to contain the full OBU and its length.
    const uint8_t *old_buffer = *buffer;
    *buffer = (uint8_t *)realloc(*buffer, *buffer_size + (size_t)obu_size);
    if (!*buffer) {
      free((void *)old_buffer);
      warn("OBU buffer alloc failed.\n");
      return 1;
    }

    data = *buffer + (*buffer_size);
    *buffer_size += obu_size;
    ret = fread(data, 1, (size_t)obu_size, infile);

    if (ret != obu_size) {
      warn("Failed to read OBU.\n");
      return 1;
    }
    *bytes_read += (size_t)obu_size;

    OBU_TYPE obu_type;
    if (get_obu_type(data[length_field_size], &obu_type) != 0) {
      warn("Invalid OBU type.\n");
      return 1;
    }

    if (obu_type == OBU_TEMPORAL_DELIMITER) {
      // Stop when a temporal delimiter is found
      // fprintf(stderr, "Found temporal delimiter, ending temporal unit\n");
      // prevent decoder to start decoding another frame from this buffer
      *bytes_read -= (size_t)obu_size;
      break;
    }

#if CONFIG_SCALABILITY
    // break if obu_extension_flag is found and enhancement_id change
    if (data[length_field_size] & 0x1) {
      const uint8_t obu_extension_header =
          data[length_field_size + OBU_HEADER_SIZE_BYTES];
      const int curr_layer_id = (obu_extension_header >> 3) & 0x3;
      if (curr_layer_id && (curr_layer_id > last_layer_id)) {
        // new enhancement layer
        *bytes_read -= (size_t)obu_size;
        const int i_obu_size = (int)obu_size;
        fseek(infile, -i_obu_size, SEEK_CUR);
        break;
      }
    }
#endif
  }

  return 0;
}

int file_is_obu(struct AvxInputContext *input_ctx) {
  uint8_t obutd[OBU_MAX_HEADER_SIZE] = { 0 };
  uint64_t size = 0;

  // Parsing of Annex B OBU streams via pipe without framing not supported. This
  // implementation requires seeking backwards in the input stream. Tell caller
  // that this input cannot be processed.
  if (!input_ctx->filename || strcmp(input_ctx->filename, "-") == 0) return 0;

  // Reading the first OBU TD to enable TU end detection at TD start.
  const size_t ret = fread(obutd, 1, OBU_MAX_HEADER_SIZE, input_ctx->file);
  if (ret != OBU_MAX_HEADER_SIZE) {
    warn("Failed to read OBU Header, not enough data to process header.\n");
    return 0;
  }

  if (aom_uleb_decode(obutd, OBU_MAX_HEADER_SIZE, &size, NULL) != 0) {
    warn("OBU size parse failed.\n");
    return 0;
  }

  const size_t obu_header_offset = aom_uleb_size_in_bytes(size);

  fseek(input_ctx->file, obu_header_offset + OBU_HEADER_SIZE_BYTES, SEEK_SET);

  if (size != 1) {
    warn("Expected first OBU size to be 1, got %d\n", size);
    return 0;
  }
  OBU_TYPE obu_type;
  if (get_obu_type(obutd[obu_header_offset], &obu_type) != 0) {
    warn("Invalid OBU type found while probing for OBU_TEMPORAL_DELIMITER.\n");
    return 0;
  }
  if (obu_type != OBU_TEMPORAL_DELIMITER) {
    warn("Expected OBU TD at file start, got %d\n", obutd[obu_header_offset]);
    return 0;
  }

  // fprintf(stderr, "Starting to parse OBU stream\n");
  return 1;
}

#endif
