/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_OPTICAL_FLOW_H_
#define AOM_AV1_ENCODER_OPTICAL_FLOW_H_

#include "config/aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_OPTICAL_FLOW_API

typedef enum { LUCAS_KANADE } OPTFLOW_METHOD;

typedef enum {
  MV_FILTER_NONE,
  MV_FILTER_SMOOTH,
  MV_FILTER_MEDIAN
} MV_FILTER_TYPE;

#define MAX_PYRAMID_LEVELS 5
// default options for optical flow
#define OPFL_WINDOW_SIZE 15
#define OPFL_PYRAMID_LEVELS 3  // total levels

// parameters specific to Lucas-Kanade
typedef struct lk_params {
  int window_size;
} LK_PARAMS;

// generic structure to contain parameters for all
// optical flow algorithms
typedef struct opfl_params {
  int pyramid_levels;
  LK_PARAMS *lk_params;
  int flags;
} OPFL_PARAMS;

#define OPFL_FLAG_SPARSE 1

void init_opfl_params(OPFL_PARAMS *opfl_params) {
  opfl_params->pyramid_levels = OPFL_PYRAMID_LEVELS;
  opfl_params->lk_params = NULL;
}

void init_lk_params(LK_PARAMS *lk_params) {
  lk_params->window_size = OPFL_WINDOW_SIZE;
}

void optical_flow(const YV12_BUFFER_CONFIG *from_frame,
                  const YV12_BUFFER_CONFIG *to_frame, const int from_frame_idx,
                  const int to_frame_idx, const int bit_depth,
                  const OPFL_PARAMS *opfl_params,
                  const MV_FILTER_TYPE mv_filter, const OPTFLOW_METHOD method,
                  MV *mvs);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_OPTICAL_FLOW_H_
