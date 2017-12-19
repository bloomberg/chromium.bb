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

#define OBU_HEADER_SIZE_BYTES 1
#if CONFIG_SCALABILITY
#define OBU_HEADER_EXTENSION_SIZE_BYTES 1
#define OBU_SEQUENCE_HEADER_BYTES 10
#endif

#if CONFIG_OBU_NO_IVF
int obu_read_temporal_unit(FILE *infile, uint8_t **buffer, size_t *bytes_read,
#if CONFIG_SCALABILITY
                           size_t *buffer_size, int last_layer_id) {
#else
                           size_t *buffer_size) {
#endif
  size_t ret;
  const size_t obu_length_header_size =
      PRE_OBU_SIZE_BYTES + OBU_HEADER_SIZE_BYTES;
  uint32_t obu_size = 0;
  uint8_t *data = NULL;

  if (feof(infile)) {
    return 1;
  }

  *buffer_size = 0;
  *bytes_read = 0;
  while (1) {
    // augment the buffer to just contain the next size field
    // and the first byte of the header
    *buffer = realloc(*buffer, (*buffer_size) + obu_length_header_size);
    data = *buffer + (*buffer_size);
    *buffer_size += obu_length_header_size;
    ret = fread(data, 1, obu_length_header_size, infile);
    if (ret == 0) {
      // fprintf(stderr, "Found end of stream, ending temporal unit\n");
      break;
    }
    if (ret != obu_length_header_size) {
      warn("Failed to read OBU Header\n");
      return 1;
    }
    *bytes_read += obu_length_header_size;

    if (((data[PRE_OBU_SIZE_BYTES] >> 3) & 0xF) == OBU_TEMPORAL_DELIMITER) {
      // Stop when a temporal delimiter is found
      // fprintf(stderr, "Found temporal delimiter, ending temporal unit\n");
      // prevent decoder to start decoding another frame from this buffer
      *bytes_read -= obu_length_header_size;
      break;
    }

#if CONFIG_SCALABILITY
    // break if obu_extension_flag is found and enhancement_id change
    if ((data[PRE_OBU_SIZE_BYTES] & 0x1)) {
      uint8_t obu_extension_header;
      int total_obu_header_size =
          (int)obu_length_header_size + OBU_HEADER_EXTENSION_SIZE_BYTES;
      int curr_layer_id;
      fread(&obu_extension_header, 1, OBU_HEADER_EXTENSION_SIZE_BYTES, infile);
      curr_layer_id = (obu_extension_header >> 3) & 0x3;
      if (curr_layer_id && (curr_layer_id > last_layer_id)) {
        // new enhancement layer
        *bytes_read -= obu_length_header_size;
        fseek(infile, -total_obu_header_size, SEEK_CUR);
        break;
      } else {
        fseek(infile, -OBU_HEADER_EXTENSION_SIZE_BYTES, SEEK_CUR);
      }
    }
#endif

    // otherwise, read the OBU payload into memory
    obu_size = mem_get_le32(data);
    // fprintf(stderr, "Found OBU of type %d and size %d\n",
    //        ((data[PRE_OBU_SIZE_BYTES] >> 3) & 0xF), obu_size);
    obu_size--;  // removing the byte of the header already read
    if (obu_size) {
      *buffer = realloc(*buffer, (*buffer_size) + obu_size);
      data = *buffer + (*buffer_size);
      *buffer_size += obu_size;
      ret = fread(data, 1, obu_size, infile);
      if (ret != obu_size) {
        warn("Failed to read OBU Payload\n");
        return 1;
      }
      *bytes_read += obu_size;
    }
  }
  return 0;
}

int file_is_obu(struct AvxInputContext *input_ctx) {
  uint8_t obutd[PRE_OBU_SIZE_BYTES + OBU_HEADER_SIZE_BYTES];
  int size;

#if !CONFIG_OBU
  warn("obudec.c requires CONFIG_OBU");
  return 0;
#endif

  // Reading the first OBU TD to enable TU end detection at TD start.
  fread(obutd, 1, PRE_OBU_SIZE_BYTES + OBU_HEADER_SIZE_BYTES, input_ctx->file);
  size = mem_get_le32(obutd);
  if (size != 1) {
    warn("Expected first OBU size to be 1, got %d", size);
    return 0;
  }
  if (((obutd[PRE_OBU_SIZE_BYTES] >> 3) & 0xF) != OBU_TEMPORAL_DELIMITER) {
    warn("Expected OBU TD at file start, got %d\n", obutd[PRE_OBU_SIZE_BYTES]);
    return 0;
  }
  // fprintf(stderr, "Starting to parse OBU stream\n");

  return 1;
}

#endif
