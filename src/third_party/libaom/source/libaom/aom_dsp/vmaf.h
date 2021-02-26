/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_VMAF_H_
#define AOM_AOM_DSP_VMAF_H_

#include <stdbool.h>
#include "aom_scale/yv12config.h"

#if CONFIG_USE_VMAF_RC
typedef struct VmafContext VmafContext;
typedef struct VmafModel VmafModel;
#endif

#if CONFIG_USE_VMAF_RC
void aom_init_vmaf_context_rc(VmafContext **vmaf_context, VmafModel *vmaf_model,
                              bool cal_vmaf_neg);
void aom_close_vmaf_context_rc(VmafContext *vmaf_context);

void aom_init_vmaf_model_rc(VmafModel **vmaf_model, const char *model_path);
void aom_close_vmaf_model_rc(VmafModel *vmaf_model);

void aom_calc_vmaf_at_index_rc(VmafContext *vmaf_context, VmafModel *vmaf_model,
                               const YV12_BUFFER_CONFIG *source,
                               const YV12_BUFFER_CONFIG *distorted,
                               int bit_depth, int frame_index, double *vmaf);
#else
void aom_calc_vmaf(const char *model_path, const YV12_BUFFER_CONFIG *source,
                   const YV12_BUFFER_CONFIG *distorted, int bit_depth,
                   double *vmaf);

void aom_calc_vmaf_multi_frame(
    void *user_data, const char *model_path,
    int (*read_frame)(float *ref_data, float *main_data, float *temp_data,
                      int stride_byte, void *user_data),
    int frame_width, int frame_height, int bit_depth, double *vmaf);
#endif  // CONFIG_USE_VMAF_RC

#endif  // AOM_AOM_DSP_VMAF_H_
