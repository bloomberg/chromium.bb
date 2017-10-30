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
#ifndef OBUDEC_H_
#define OBUDEC_H_

#include "./tools_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int file_is_obu(struct AvxInputContext *input_ctx);

int obu_read_temporal_unit(FILE *infile, uint8_t **buffer, size_t *bytes_read,
                           size_t *buffer_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // OBUDEC_H_
