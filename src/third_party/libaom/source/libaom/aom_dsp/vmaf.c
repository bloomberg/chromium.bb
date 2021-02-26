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

#include "aom_dsp/vmaf.h"

#include <assert.h>
#if !CONFIG_USE_VMAF_RC
#include <libvmaf.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#if CONFIG_USE_VMAF_RC
#include <libvmaf/libvmaf.rc.h>
#endif

#include "aom_dsp/blend.h"
#include "aom_ports/system_state.h"

static void vmaf_fatal_error(const char *message) {
  fprintf(stderr, "Fatal error: %s\n", message);
  exit(EXIT_FAILURE);
}

#if !CONFIG_USE_VMAF_RC
typedef struct FrameData {
  const YV12_BUFFER_CONFIG *source;
  const YV12_BUFFER_CONFIG *distorted;
  int frame_set;
  int bit_depth;
} FrameData;

// A callback function used to pass data to VMAF.
// Returns 0 after reading a frame.
// Returns 2 when there is no more frame to read.
static int read_frame(float *ref_data, float *main_data, float *temp_data,
                      int stride, void *user_data) {
  FrameData *frames = (FrameData *)user_data;

  if (!frames->frame_set) {
    const int width = frames->source->y_width;
    const int height = frames->source->y_height;
    assert(width == frames->distorted->y_width);
    assert(height == frames->distorted->y_height);

    if (frames->source->flags & YV12_FLAG_HIGHBITDEPTH) {
      const float scale_factor = 1.0f / (float)(1 << (frames->bit_depth - 8));
      uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(frames->source->y_buffer);
      uint16_t *main_ptr = CONVERT_TO_SHORTPTR(frames->distorted->y_buffer);

      for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
          ref_data[col] = scale_factor * (float)ref_ptr[col];
        }
        ref_ptr += frames->source->y_stride;
        ref_data += stride / sizeof(*ref_data);
      }

      for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
          main_data[col] = scale_factor * (float)main_ptr[col];
        }
        main_ptr += frames->distorted->y_stride;
        main_data += stride / sizeof(*main_data);
      }
    } else {
      uint8_t *ref_ptr = frames->source->y_buffer;
      uint8_t *main_ptr = frames->distorted->y_buffer;

      for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
          ref_data[col] = (float)ref_ptr[col];
        }
        ref_ptr += frames->source->y_stride;
        ref_data += stride / sizeof(*ref_data);
      }

      for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
          main_data[col] = (float)main_ptr[col];
        }
        main_ptr += frames->distorted->y_stride;
        main_data += stride / sizeof(*main_data);
      }
    }
    frames->frame_set = 1;
    return 0;
  }

  (void)temp_data;
  return 2;
}

void aom_calc_vmaf(const char *model_path, const YV12_BUFFER_CONFIG *source,
                   const YV12_BUFFER_CONFIG *distorted, const int bit_depth,
                   double *const vmaf) {
  aom_clear_system_state();
  const int width = source->y_width;
  const int height = source->y_height;
  FrameData frames = { source, distorted, 0, bit_depth };
  char *fmt = bit_depth == 10 ? "yuv420p10le" : "yuv420p";
  double vmaf_score;
  const int ret =
      compute_vmaf(&vmaf_score, fmt, width, height, read_frame,
                   /*user_data=*/&frames, (char *)model_path,
                   /*log_path=*/NULL, /*log_fmt=*/NULL, /*disable_clip=*/1,
                   /*disable_avx=*/0, /*enable_transform=*/0,
                   /*phone_model=*/0, /*do_psnr=*/0, /*do_ssim=*/0,
                   /*do_ms_ssim=*/0, /*pool_method=*/NULL, /*n_thread=*/0,
                   /*n_subsample=*/1, /*enable_conf_interval=*/0);
  if (ret) vmaf_fatal_error("Failed to compute VMAF scores.");

  aom_clear_system_state();
  *vmaf = vmaf_score;
}

void aom_calc_vmaf_multi_frame(void *user_data, const char *model_path,
                               int (*rd_frm)(float *ref_data, float *main_data,
                                             float *temp_data, int stride_byte,
                                             void *user_data),
                               int frame_width, int frame_height, int bit_depth,
                               double *vmaf) {
  aom_clear_system_state();

  char *fmt = bit_depth == 10 ? "yuv420p10le" : "yuv420p";
  int log_path_length = snprintf(NULL, 0, "vmaf_scores_%d.xml", getpid()) + 1;
  char *log_path = malloc(log_path_length);
  snprintf(log_path, log_path_length, "vmaf_scores_%d.xml", getpid());
  double vmaf_score;
  const int ret =
      compute_vmaf(&vmaf_score, fmt, frame_width, frame_height, rd_frm,
                   /*user_data=*/user_data, (char *)model_path,
                   /*log_path=*/log_path, /*log_fmt=*/NULL, /*disable_clip=*/0,
                   /*disable_avx=*/0, /*enable_transform=*/0,
                   /*phone_model=*/0, /*do_psnr=*/0, /*do_ssim=*/0,
                   /*do_ms_ssim=*/0, /*pool_method=*/NULL, /*n_thread=*/0,
                   /*n_subsample=*/1, /*enable_conf_interval=*/0);
  FILE *vmaf_log = fopen(log_path, "r");
  free(log_path);
  log_path = NULL;
  if (vmaf_log == NULL || ret) {
    vmaf_fatal_error("Failed to compute VMAF scores.");
  }

  int frame_index = 0;
  char buf[512];
  while (fgets(buf, 511, vmaf_log) != NULL) {
    if (memcmp(buf, "\t\t<frame ", 9) == 0) {
      char *p = strstr(buf, "vmaf=");
      if (p != NULL && p[5] == '"') {
        char *p2 = strstr(&p[6], "\"");
        *p2 = '\0';
        const double score = atof(&p[6]);
        if (score < 0.0 || score > 100.0) {
          vmaf_fatal_error("Failed to compute VMAF scores.");
        }
        vmaf[frame_index++] = score;
      }
    }
  }
  fclose(vmaf_log);

  aom_clear_system_state();
}
#endif

#if CONFIG_USE_VMAF_RC
void aom_init_vmaf_model_rc(VmafModel **vmaf_model, const char *model_path) {
  if (*vmaf_model != NULL) return;
  VmafModelConfig model_cfg;
  model_cfg.flags = VMAF_MODEL_FLAG_DISABLE_CLIP;
  model_cfg.name = "vmaf";
  model_cfg.path = (char *)model_path;

  if (vmaf_model_load_from_path(vmaf_model, &model_cfg)) {
    vmaf_fatal_error("Failed to load VMAF model.");
  }
}

void aom_close_vmaf_model_rc(VmafModel *vmaf_model) {
  vmaf_model_destroy(vmaf_model);
}

static void copy_picture(const int bit_depth, const YV12_BUFFER_CONFIG *src,
                         VmafPicture *dst) {
  const int width = src->y_width;
  const int height = src->y_height;

  if (bit_depth > 8) {
    uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src->y_buffer);
    uint16_t *dst_ptr = dst->data[0];

    for (int row = 0; row < height; ++row) {
      memcpy(dst_ptr, src_ptr, width * sizeof(dst_ptr[0]));
      src_ptr += src->y_stride;
      dst_ptr += dst->stride[0] / 2;
    }
  } else {
    uint8_t *src_ptr = src->y_buffer;
    uint8_t *dst_ptr = (uint8_t *)dst->data[0];

    for (int row = 0; row < height; ++row) {
      memcpy(dst_ptr, src_ptr, width * sizeof(dst_ptr[0]));
      src_ptr += src->y_stride;
      dst_ptr += dst->stride[0];
    }
  }
}

void aom_init_vmaf_context_rc(VmafContext **vmaf_context, VmafModel *vmaf_model,
                              bool cal_vmaf_neg) {
  VmafConfiguration cfg;
  cfg.log_level = VMAF_LOG_LEVEL_NONE;
  cfg.n_threads = 0;
  cfg.n_subsample = 0;
  cfg.cpumask = 0;

  if (vmaf_init(vmaf_context, cfg)) {
    vmaf_fatal_error("Failed to init VMAF context.");
  }

  if (vmaf_use_features_from_model(*vmaf_context, vmaf_model)) {
    vmaf_fatal_error("Failed to load feature extractors from VMAF model.");
  }

  if (cal_vmaf_neg) {
    VmafFeatureDictionary *vif_feature = NULL;
    vmaf_feature_dictionary_set(&vif_feature, "vif_enhn_gain_limit", "1.0");
    if (vmaf_use_feature(*vmaf_context, "float_vif", vif_feature)) {
      vmaf_fatal_error("Failed to use feature float_vif.");
    }

    VmafFeatureDictionary *adm_feature = NULL;
    vmaf_feature_dictionary_set(&adm_feature, "adm_enhn_gain_limit", "1.0");
    if (vmaf_use_feature(*vmaf_context, "float_adm", adm_feature)) {
      vmaf_fatal_error("Failed to use feature float_adm.");
    }
  }

  VmafFeatureDictionary *motion_force_zero = NULL;
  vmaf_feature_dictionary_set(&motion_force_zero, "motion_force_zero", "true");
  if (vmaf_use_feature(*vmaf_context, "float_motion", motion_force_zero)) {
    vmaf_fatal_error("Failed to use feature float_motion.");
  }
}

void aom_close_vmaf_context_rc(VmafContext *vmaf_context) {
  if (vmaf_close(vmaf_context)) {
    vmaf_fatal_error("Failed to close VMAF context.");
  }
}

void aom_calc_vmaf_at_index_rc(VmafContext *vmaf_context, VmafModel *vmaf_model,
                               const YV12_BUFFER_CONFIG *source,
                               const YV12_BUFFER_CONFIG *distorted,
                               int bit_depth, int frame_index, double *vmaf) {
  VmafPicture ref, dist;
  if (vmaf_picture_alloc(&ref, VMAF_PIX_FMT_YUV420P, bit_depth, source->y_width,
                         source->y_height) ||
      vmaf_picture_alloc(&dist, VMAF_PIX_FMT_YUV420P, bit_depth,
                         source->y_width, source->y_height)) {
    vmaf_fatal_error("Failed to alloc VMAF pictures.");
  }
  copy_picture(bit_depth, source, &ref);
  copy_picture(bit_depth, distorted, &dist);
  if (vmaf_read_pictures(vmaf_context, &ref, &dist,
                         /*picture index=*/frame_index)) {
    vmaf_fatal_error("Failed to read VMAF pictures.");
  }

  vmaf_picture_unref(&ref);
  vmaf_picture_unref(&dist);

  vmaf_score_at_index(vmaf_context, vmaf_model, vmaf, frame_index);
}

#endif  // CONFIG_USE_VMAF_RC
