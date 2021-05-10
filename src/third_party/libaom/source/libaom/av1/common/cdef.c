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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/cdef.h"
#include "av1/common/cdef_block.h"
#include "av1/common/reconinter.h"

enum { TOP, LEFT, BOTTOM, RIGHT, BOUNDARIES } UENUM1BYTE(BOUNDARY);

/*!\brief Parameters related to CDEF Block */
typedef struct {
  uint16_t *src;
  uint8_t *dst;
  uint16_t *colbuf[MAX_MB_PLANE];
  cdef_list dlist[MI_SIZE_64X64 * MI_SIZE_64X64];

  int xdec;
  int ydec;
  int mi_wide_l2;
  int mi_high_l2;
  int frame_boundary[BOUNDARIES];

  int damping;
  int coeff_shift;
  int level;
  int sec_strength;
  int cdef_count;
  int is_zero_level;
  int dir[CDEF_NBLOCKS][CDEF_NBLOCKS];
  int var[CDEF_NBLOCKS][CDEF_NBLOCKS];

  int dst_stride;
  int coffset;
  int roffset;
} CdefBlockInfo;

static int is_8x8_block_skip(MB_MODE_INFO **grid, int mi_row, int mi_col,
                             int mi_stride) {
  MB_MODE_INFO **mbmi = grid + mi_row * mi_stride + mi_col;
  for (int r = 0; r < mi_size_high[BLOCK_8X8]; ++r, mbmi += mi_stride) {
    for (int c = 0; c < mi_size_wide[BLOCK_8X8]; ++c) {
      if (!mbmi[c]->skip_txfm) return 0;
    }
  }

  return 1;
}

int av1_cdef_compute_sb_list(const CommonModeInfoParams *const mi_params,
                             int mi_row, int mi_col, cdef_list *dlist,
                             BLOCK_SIZE bs) {
  MB_MODE_INFO **grid = mi_params->mi_grid_base;
  int maxc = mi_params->mi_cols - mi_col;
  int maxr = mi_params->mi_rows - mi_row;

  if (bs == BLOCK_128X128 || bs == BLOCK_128X64)
    maxc = AOMMIN(maxc, MI_SIZE_128X128);
  else
    maxc = AOMMIN(maxc, MI_SIZE_64X64);
  if (bs == BLOCK_128X128 || bs == BLOCK_64X128)
    maxr = AOMMIN(maxr, MI_SIZE_128X128);
  else
    maxr = AOMMIN(maxr, MI_SIZE_64X64);

  const int r_step = 2;  // mi_size_high[BLOCK_8X8]
  const int c_step = 2;  // mi_size_wide[BLOCK_8X8]
  const int r_shift = 1;
  const int c_shift = 1;
  int count = 0;
  for (int r = 0; r < maxr; r += r_step) {
    for (int c = 0; c < maxc; c += c_step) {
      if (!is_8x8_block_skip(grid, mi_row + r, mi_col + c,
                             mi_params->mi_stride)) {
        dlist[count].by = r >> r_shift;
        dlist[count].bx = c >> c_shift;
        count++;
      }
    }
  }
  return count;
}

void cdef_copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride,
                                     const uint8_t *src, int sstride, int v,
                                     int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void cdef_copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride,
                                      const uint16_t *src, int sstride, int v,
                                      int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

static void copy_sb8_16(AV1_COMMON *cm, uint16_t *dst, int dstride,
                        const uint8_t *src, int src_voffset, int src_hoffset,
                        int sstride, int vsize, int hsize) {
  if (cm->seq_params.use_highbitdepth) {
    const uint16_t *base =
        &CONVERT_TO_SHORTPTR(src)[src_voffset * sstride + src_hoffset];
    cdef_copy_rect8_16bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
  } else {
    const uint8_t *base = &src[src_voffset * sstride + src_hoffset];
    cdef_copy_rect8_8bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
  }
}

static INLINE void fill_rect(uint16_t *dst, int dstride, int v, int h,
                             uint16_t x) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = x;
    }
  }
}

static INLINE void copy_rect(uint16_t *dst, int dstride, const uint16_t *src,
                             int sstride, int v, int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

// Prepares intermediate input buffer for CDEF.
// Inputs:
//   cm: Pointer to common structure.
//   fb_info: Pointer to the CDEF block-level parameter structure.
//   linebuf: Top feedback buffer for CDEF.
//   cdef_left: Left block is filtered or not.
//   fbc, fbr: col and row index of a block.
//   plane: plane index Y/CB/CR.
//   prev_row_cdef: Top blocks are filtered or not.
// Returns:
//   Nothing will be returned.
static void cdef_prepare_fb(AV1_COMMON *cm, CdefBlockInfo *fb_info,
                            uint16_t **linebuf, const int *cdef_left, int fbc,
                            int fbr, uint8_t plane,
                            unsigned char *prev_row_cdef) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  uint16_t *src = fb_info->src;
  const int stride = (mi_params->mi_cols << MI_SIZE_LOG2) + 2 * CDEF_HBORDER;
  const int nvfb = (mi_params->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int nhfb = (mi_params->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  int cstart = 0;
  if (!*cdef_left) cstart = -CDEF_HBORDER;
  int rend, cend;
  int nhb = AOMMIN(MI_SIZE_64X64, mi_params->mi_cols - MI_SIZE_64X64 * fbc);
  int nvb = AOMMIN(MI_SIZE_64X64, mi_params->mi_rows - MI_SIZE_64X64 * fbr);
  int hsize = nhb << fb_info->mi_wide_l2;
  int vsize = nvb << fb_info->mi_high_l2;

  if (fbc == nhfb - 1)
    cend = hsize;
  else
    cend = hsize + CDEF_HBORDER;

  if (fbr == nvfb - 1)
    rend = vsize;
  else
    rend = vsize + CDEF_VBORDER;

  if (fbc == nhfb - 1) {
    /* On the last superblock column, fill in the right border with
    CDEF_VERY_LARGE to avoid filtering with the outside. */
    fill_rect(&src[cend + CDEF_HBORDER], CDEF_BSTRIDE, rend + CDEF_VBORDER,
              hsize + CDEF_HBORDER - cend, CDEF_VERY_LARGE);
  }
  if (fbr == nvfb - 1) {
    /* On the last superblock row, fill in the bottom border with
    CDEF_VERY_LARGE to avoid filtering with the outside. */
    fill_rect(&src[(rend + CDEF_VBORDER) * CDEF_BSTRIDE], CDEF_BSTRIDE,
              CDEF_VBORDER, hsize + 2 * CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  /* Copy in the pixels we need from the current superblock for
  deringing.*/
  copy_sb8_16(cm, &src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER + cstart],
              CDEF_BSTRIDE, fb_info->dst, fb_info->roffset,
              fb_info->coffset + cstart, fb_info->dst_stride, rend,
              cend - cstart);
  if (!prev_row_cdef[fbc]) {
    copy_sb8_16(cm, &src[CDEF_HBORDER], CDEF_BSTRIDE, fb_info->dst,
                fb_info->roffset - CDEF_VBORDER, fb_info->coffset,
                fb_info->dst_stride, CDEF_VBORDER, hsize);
  } else if (fbr > 0) {
    copy_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE,
              &linebuf[plane][fb_info->coffset], stride, CDEF_VBORDER, hsize);
  } else {
    fill_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER, hsize,
              CDEF_VERY_LARGE);
  }
  if (!prev_row_cdef[fbc - 1]) {
    copy_sb8_16(cm, src, CDEF_BSTRIDE, fb_info->dst,
                fb_info->roffset - CDEF_VBORDER,
                fb_info->coffset - CDEF_HBORDER, fb_info->dst_stride,
                CDEF_VBORDER, CDEF_HBORDER);
  } else if (fbr > 0 && fbc > 0) {
    copy_rect(src, CDEF_BSTRIDE,
              &linebuf[plane][fb_info->coffset - CDEF_HBORDER], stride,
              CDEF_VBORDER, CDEF_HBORDER);
  } else {
    fill_rect(src, CDEF_BSTRIDE, CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  if (!prev_row_cdef[fbc + 1]) {
    copy_sb8_16(cm, &src[CDEF_HBORDER + hsize], CDEF_BSTRIDE, fb_info->dst,
                fb_info->roffset - CDEF_VBORDER, fb_info->coffset + hsize,
                fb_info->dst_stride, CDEF_VBORDER, CDEF_HBORDER);
  } else if (fbr > 0 && fbc < nhfb - 1) {
    copy_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              &linebuf[plane][fb_info->coffset + hsize], stride, CDEF_VBORDER,
              CDEF_HBORDER);
  } else {
    fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER,
              CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  if (*cdef_left) {
    /* If we deringed the superblock on the left then we need to copy in
    saved pixels. */
    copy_rect(src, CDEF_BSTRIDE, fb_info->colbuf[plane], CDEF_HBORDER,
              rend + CDEF_VBORDER, CDEF_HBORDER);
  }
  /* Saving pixels in case we need to dering the superblock on the
  right. */
  copy_rect(fb_info->colbuf[plane], CDEF_HBORDER, src + hsize, CDEF_BSTRIDE,
            rend + CDEF_VBORDER, CDEF_HBORDER);
  copy_sb8_16(cm, &linebuf[plane][fb_info->coffset], stride, fb_info->dst,
              (MI_SIZE_64X64 << fb_info->mi_high_l2) * (fbr + 1) - CDEF_VBORDER,
              fb_info->coffset, fb_info->dst_stride, CDEF_VBORDER, hsize);

  if (fb_info->frame_boundary[TOP]) {
    fill_rect(src, CDEF_BSTRIDE, CDEF_VBORDER, hsize + 2 * CDEF_HBORDER,
              CDEF_VERY_LARGE);
  }
  if (fb_info->frame_boundary[LEFT]) {
    fill_rect(src, CDEF_BSTRIDE, vsize + 2 * CDEF_VBORDER, CDEF_HBORDER,
              CDEF_VERY_LARGE);
  }
  if (fb_info->frame_boundary[BOTTOM]) {
    fill_rect(&src[(vsize + CDEF_VBORDER) * CDEF_BSTRIDE], CDEF_BSTRIDE,
              CDEF_VBORDER, hsize + 2 * CDEF_HBORDER, CDEF_VERY_LARGE);
  }
  if (fb_info->frame_boundary[RIGHT]) {
    fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
              vsize + 2 * CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
  }
}

static INLINE void cdef_filter_fb(CdefBlockInfo *fb_info, uint8_t plane,
                                  uint8_t use_highbitdepth) {
  int offset = fb_info->dst_stride * fb_info->roffset + fb_info->coffset;
  if (use_highbitdepth) {
    av1_cdef_filter_fb(
        NULL, CONVERT_TO_SHORTPTR(fb_info->dst + offset), fb_info->dst_stride,
        &fb_info->src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER],
        fb_info->xdec, fb_info->ydec, fb_info->dir, NULL, fb_info->var, plane,
        fb_info->dlist, fb_info->cdef_count, fb_info->level,
        fb_info->sec_strength, fb_info->damping, fb_info->coeff_shift);
  } else {
    av1_cdef_filter_fb(
        fb_info->dst + offset, NULL, fb_info->dst_stride,
        &fb_info->src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER],
        fb_info->xdec, fb_info->ydec, fb_info->dir, NULL, fb_info->var, plane,
        fb_info->dlist, fb_info->cdef_count, fb_info->level,
        fb_info->sec_strength, fb_info->damping, fb_info->coeff_shift);
  }
}

// Initializes block-level parameters for CDEF.
static INLINE void cdef_init_fb_col(MACROBLOCKD *xd,
                                    const CdefInfo *const cdef_info,
                                    CdefBlockInfo *fb_info,
                                    const int mbmi_cdef_strength, int fbc,
                                    int fbr, uint8_t plane) {
  if (plane == AOM_PLANE_Y) {
    fb_info->level =
        cdef_info->cdef_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
    fb_info->sec_strength =
        cdef_info->cdef_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
    fb_info->sec_strength += fb_info->sec_strength == 3;
    int uv_level =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
    int uv_sec_strength =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
    uv_sec_strength += uv_sec_strength == 3;
    fb_info->is_zero_level = (fb_info->level == 0) &&
                             (fb_info->sec_strength == 0) && (uv_level == 0) &&
                             (uv_sec_strength == 0);
  } else {
    fb_info->level =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
    fb_info->sec_strength =
        cdef_info->cdef_uv_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
    fb_info->sec_strength += fb_info->sec_strength == 3;
  }
  fb_info->dst = xd->plane[plane].dst.buf;
  fb_info->dst_stride = xd->plane[plane].dst.stride;

  fb_info->xdec = xd->plane[plane].subsampling_x;
  fb_info->ydec = xd->plane[plane].subsampling_y;
  fb_info->mi_wide_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_x;
  fb_info->mi_high_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
  fb_info->roffset = MI_SIZE_64X64 * fbr << fb_info->mi_high_l2;
  fb_info->coffset = MI_SIZE_64X64 * fbc << fb_info->mi_wide_l2;
}

static bool cdef_fb_col(AV1_COMMON *cm, MACROBLOCKD *xd, CdefBlockInfo *fb_info,
                        int fbc, int fbr, int *cdef_left, uint16_t **linebuf,
                        unsigned char *prev_row_cdef) {
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const int mbmi_cdef_strength =
      mi_params
          ->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                         MI_SIZE_64X64 * fbc]
          ->cdef_strength;
  const int num_planes = av1_num_planes(cm);

  if (mi_params->mi_grid_base[MI_SIZE_64X64 * fbr * mi_params->mi_stride +
                              MI_SIZE_64X64 * fbc] == NULL ||
      mbmi_cdef_strength == -1) {
    *cdef_left = 0;
    return 0;
  }
  for (uint8_t plane = 0; plane < num_planes; plane++) {
    cdef_init_fb_col(xd, &cm->cdef_info, fb_info, mbmi_cdef_strength, fbc, fbr,
                     plane);
    if (fb_info->is_zero_level ||
        (fb_info->cdef_count = av1_cdef_compute_sb_list(
             mi_params, fbr * MI_SIZE_64X64, fbc * MI_SIZE_64X64,
             fb_info->dlist, BLOCK_64X64)) == 0) {
      *cdef_left = 0;
      return 0;
    }
    cdef_prepare_fb(cm, fb_info, linebuf, cdef_left, fbc, fbr, plane,
                    prev_row_cdef);
    cdef_filter_fb(fb_info, plane, cm->seq_params.use_highbitdepth);
  }
  *cdef_left = 1;
  return 1;
}

static INLINE void cdef_init_fb_row(CdefBlockInfo *fb_info, int mi_rows,
                                    int fbr) {
  const int nvfb = (mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  // for the current filter block, it's top left corner mi structure (mi_tl)
  // is first accessed to check whether the top and left boundaries are
  // frame boundaries. Then bottom-left and top-right mi structures are
  // accessed to check whether the bottom and right boundaries
  // (respectively) are frame boundaries.
  //
  // Note that we can't just check the bottom-right mi structure - eg. if
  // we're at the right-hand edge of the frame but not the bottom, then
  // the bottom-right mi is NULL but the bottom-left is not.
  fb_info->frame_boundary[TOP] = (MI_SIZE_64X64 * fbr == 0) ? 1 : 0;
  if (fbr != nvfb - 1)
    fb_info->frame_boundary[BOTTOM] =
        (MI_SIZE_64X64 * (fbr + 1) == mi_rows) ? 1 : 0;
  else
    fb_info->frame_boundary[BOTTOM] = 1;
}

static void cdef_fb_row(AV1_COMMON *cm, MACROBLOCKD *xd, CdefBlockInfo *fb_info,
                        uint16_t **linebuf, int fbr,
                        unsigned char *curr_row_cdef,
                        unsigned char *prev_row_cdef) {
  int cdef_left = 1;
  const int nhfb = (cm->mi_params.mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  cdef_init_fb_row(fb_info, cm->mi_params.mi_rows, fbr);
  for (int fbc = 0; fbc < nhfb; fbc++) {
    fb_info->frame_boundary[LEFT] = (MI_SIZE_64X64 * fbc == 0) ? 1 : 0;
    if (fbc != nhfb - 1)
      fb_info->frame_boundary[RIGHT] =
          (MI_SIZE_64X64 * (fbc + 1) == cm->mi_params.mi_cols) ? 1 : 0;
    else
      fb_info->frame_boundary[RIGHT] = 1;
    curr_row_cdef[fbc] = cdef_fb_col(cm, xd, fb_info, fbc, fbr, &cdef_left,
                                     linebuf, prev_row_cdef);
  }
}

// Initialize the frame-level CDEF parameters.
// Inputs:
//   frame: Pointer to input frame buffer.
//   cm: Pointer to common structure.
//   xd: Pointer to common current coding block structure.
//   fb_info: Pointer to the CDEF block-level parameter structure.
//   src: Intermediate input buffer for CDEF.
//   colbuf: Left feedback buffer for CDEF.
//   linebuf: Top feedback buffer for CDEF.
// Returns:
//   Nothing will be returned.
static void cdef_prepare_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                               MACROBLOCKD *xd, CdefBlockInfo *fb_info,
                               uint16_t *src, uint16_t **colbuf,
                               uint16_t **linebuf) {
  const int num_planes = av1_num_planes(cm);
  const int stride = (cm->mi_params.mi_cols << MI_SIZE_LOG2) + 2 * CDEF_HBORDER;
  av1_setup_dst_planes(xd->plane, cm->seq_params.sb_size, frame, 0, 0, 0,
                       num_planes);

  for (uint8_t plane = 0; plane < num_planes; plane++) {
    linebuf[plane] = aom_malloc(sizeof(*linebuf) * CDEF_VBORDER * stride);
    const int mi_high_l2 = MI_SIZE_LOG2 - xd->plane[plane].subsampling_y;
    const int block_height = (MI_SIZE_64X64 << mi_high_l2) + 2 * CDEF_VBORDER;
    colbuf[plane] = aom_malloc(
        sizeof(*colbuf) *
        ((CDEF_BLOCKSIZE << (MI_SIZE_LOG2 - xd->plane[plane].subsampling_y)) +
         2 * CDEF_VBORDER) *
        CDEF_HBORDER);
    fill_rect(colbuf[plane], CDEF_HBORDER, block_height, CDEF_HBORDER,
              CDEF_VERY_LARGE);
    fb_info->colbuf[plane] = colbuf[plane];
  }

  fb_info->src = src;
  fb_info->damping = cm->cdef_info.cdef_damping;
  fb_info->coeff_shift = AOMMAX(cm->seq_params.bit_depth - 8, 0);
  memset(fb_info->dir, 0, sizeof(fb_info->dir));
  memset(fb_info->var, 0, sizeof(fb_info->var));
}

static void cdef_free(unsigned char *row_cdef, uint16_t **colbuf,
                      uint16_t **linebuf, const int num_planes) {
  aom_free(row_cdef);
  for (uint8_t plane = 0; plane < num_planes; plane++) {
    aom_free(colbuf[plane]);
    aom_free(linebuf[plane]);
  }
}

// Perform CDEF on input frame.
// Inputs:
//   frame: Pointer to input frame buffer.
//   cm: Pointer to common structure.
//   xd: Pointer to common current coding block structure.
// Returns:
//   Nothing will be returned.
void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                    MACROBLOCKD *xd) {
  DECLARE_ALIGNED(16, uint16_t, src[CDEF_INBUF_SIZE]);
  uint16_t *colbuf[MAX_MB_PLANE] = { NULL };
  uint16_t *linebuf[MAX_MB_PLANE] = { NULL };
  CdefBlockInfo fb_info;
  unsigned char *row_cdef, *prev_row_cdef, *curr_row_cdef;
  const int num_planes = av1_num_planes(cm);
  const int nvfb = (cm->mi_params.mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int nhfb = (cm->mi_params.mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;

  row_cdef = aom_malloc(sizeof(*row_cdef) * (nhfb + 2) * 2);
  memset(row_cdef, 1, sizeof(*row_cdef) * (nhfb + 2) * 2);
  prev_row_cdef = row_cdef + 1;
  curr_row_cdef = prev_row_cdef + nhfb + 2;
  cdef_prepare_frame(frame, cm, xd, &fb_info, src, colbuf, linebuf);

  for (int fbr = 0; fbr < nvfb; fbr++) {
    unsigned char *tmp;
    cdef_fb_row(cm, xd, &fb_info, linebuf, fbr, curr_row_cdef, prev_row_cdef);
    tmp = prev_row_cdef;
    prev_row_cdef = curr_row_cdef;
    curr_row_cdef = tmp;
  }
  cdef_free(row_cdef, colbuf, linebuf, num_planes);
}
