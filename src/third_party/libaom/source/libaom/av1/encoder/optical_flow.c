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
#include <math.h>
#include <limits.h>

#include "config/aom_config.h"
#include "av1/common/av1_common_int.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/mathutils.h"
#include "av1/encoder/optical_flow.h"
#include "av1/encoder/reconinter_enc.h"
#include "aom_mem/aom_mem.h"

#if CONFIG_OPTICAL_FLOW_API

// Helper function to determine whether a frame is encoded with high bit-depth.
static INLINE int is_frame_high_bitdepth(const YV12_BUFFER_CONFIG *frame) {
  return (frame->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
}

// Helper function to determine whether optical flow method is sparse.
static INLINE int is_sparse(const OPFL_PARAMS *opfl_params) {
  return (opfl_params->flags & OPFL_FLAG_SPARSE) ? 1 : 0;
}

typedef struct LOCALMV {
  double row;
  double col;
} LOCALMV;

void gradients_over_window(const YV12_BUFFER_CONFIG *frame,
                           const YV12_BUFFER_CONFIG *ref_frame,
                           const double x_coord, const double y_coord,
                           const int window_size, const int bit_depth,
                           double *ix, double *iy, double *it, LOCALMV *mv);

// coefficients for bilinear interpolation on unit square
int pixel_interp(const double x, const double y, const double b00,
                 const double b01, const double b10, const double b11) {
  const int xint = (int)x;
  const int yint = (int)y;
  const double xdec = x - xint;
  const double ydec = y - yint;
  const double a = (1 - xdec) * (1 - ydec);
  const double b = xdec * (1 - ydec);
  const double c = (1 - xdec) * ydec;
  const double d = xdec * ydec;
  // if x, y are already integers, this results to b00
  int interp = (int)round(a * b00 + b * b01 + c * b10 + d * b11);
  return interp;
}
// bilinear interpolation to find subpixel values
int get_subpixels(const YV12_BUFFER_CONFIG *frame, int *pred, const int w,
                  const int h, LOCALMV mv, const double x_coord,
                  const double y_coord) {
  double left = x_coord + mv.row;
  double top = y_coord + mv.col;
  const int fromedge = 2;
  const int height = frame->y_crop_height;
  const int width = frame->y_crop_width;
  if (left < 1) left = 1;
  if (top < 1) top = 1;
  // could use elements past boundary where stride > width
  if (top > height - fromedge) top = height - fromedge;
  if (left > width - fromedge) left = width - fromedge;
  const uint8_t *buf = frame->y_buffer;
  const int s = frame->y_stride;
  int prev = -1;

  int xint;
  int yint;
  int idx = 0;
  for (int y = prev; y < prev + h; y++) {
    for (int x = prev; x < prev + w; x++) {
      double xx = left + x;
      double yy = top + y;
      xint = (int)xx;
      yint = (int)yy;
      int interp = pixel_interp(
          xx, yy, buf[yint * s + xint], buf[yint * s + (xint + 1)],
          buf[(yint + 1) * s + xint], buf[(yint + 1) * s + (xint + 1)]);
      pred[idx++] = interp;
    }
  }
  return 0;
}
// Scharr filter to compute spatial gradient
void spatial_gradient(const YV12_BUFFER_CONFIG *frame, const int x_coord,
                      const int y_coord, const int direction,
                      double *derivative) {
  double *filter;
  // Scharr filters
  double gx[9] = { -3, 0, 3, -10, 0, 10, -3, 0, 3 };
  double gy[9] = { -3, -10, -3, 0, 0, 0, 3, 10, 3 };
  if (direction == 0) {  // x direction
    filter = gx;
  } else {  // y direction
    filter = gy;
  }
  int idx = 0;
  double d = 0;
  for (int yy = -1; yy <= 1; yy++) {
    for (int xx = -1; xx <= 1; xx++) {
      d += filter[idx] *
           frame->y_buffer[(y_coord + yy) * frame->y_stride + (x_coord + xx)];
      idx++;
    }
  }
  // normalization scaling factor for scharr
  *derivative = d / 32.0;
}
// Determine the spatial gradient at subpixel locations
// For example, when reducing images for pyramidal LK,
// corners found in original image may be at subpixel locations.
void gradient_interp(double *fullpel_deriv, const double x_coord,
                     const double y_coord, const int w, const int h,
                     double *derivative) {
  const int xint = (int)x_coord;
  const int yint = (int)y_coord;
  double interp;
  if (xint + 1 > w - 1 || yint + 1 > h - 1) {
    interp = fullpel_deriv[yint * w + xint];
  } else {
    interp = pixel_interp(x_coord, y_coord, fullpel_deriv[yint * w + xint],
                          fullpel_deriv[yint * w + (xint + 1)],
                          fullpel_deriv[(yint + 1) * w + xint],
                          fullpel_deriv[(yint + 1) * w + (xint + 1)]);
  }

  *derivative = interp;
}

void temporal_gradient(const YV12_BUFFER_CONFIG *frame,
                       const YV12_BUFFER_CONFIG *frame2, const double x_coord,
                       const double y_coord, const int bit_depth,
                       double *derivative, LOCALMV *mv) {
  // TODO(any): this is a roundabout way of enforcing build_one_inter_pred
  // to use the 8-tap filter (instead of lower). it would be more
  // efficient to apply the filter only at 1 pixel instead of 25 pixels.
  const int w = 5;
  const int h = 5;
  uint8_t pred1[25];
  uint8_t pred2[25];

  const int y = (int)y_coord;
  const int x = (int)x_coord;
  const double ydec = y_coord - y;
  const double xdec = x_coord - x;
  const int is_intrabc = 0;  // Is intra-copied?
  const int is_high_bitdepth = is_frame_high_bitdepth(frame2);
  const int subsampling_x = 0, subsampling_y = 0;  // for y-buffer
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(MULTITAP_SHARP);
  const int plane = 0;  // y-plane
  const struct buf_2d ref_buf2 = { NULL, frame2->y_buffer, frame2->y_crop_width,
                                   frame2->y_crop_height, frame2->y_stride };
  struct scale_factors scale;
  av1_setup_scale_factors_for_frame(&scale, frame->y_crop_width,
                                    frame->y_crop_height, frame->y_crop_width,
                                    frame->y_crop_height);
  InterPredParams inter_pred_params;
  av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                        subsampling_y, bit_depth, is_high_bitdepth, is_intrabc,
                        &scale, &ref_buf2, interp_filters);
  inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
  MV newmv = { .row = (int16_t)round((mv->row + xdec) * 8),
               .col = (int16_t)round((mv->col + ydec) * 8) };
  av1_enc_build_one_inter_predictor(pred2, w, &newmv, &inter_pred_params);
  const struct buf_2d ref_buf1 = { NULL, frame->y_buffer, frame->y_crop_width,
                                   frame->y_crop_height, frame->y_stride };
  av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                        subsampling_y, bit_depth, is_high_bitdepth, is_intrabc,
                        &scale, &ref_buf1, interp_filters);
  inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
  MV zeroMV = { .row = (int16_t)round(xdec * 8),
                .col = (int16_t)round(ydec * 8) };
  av1_enc_build_one_inter_predictor(pred1, w, &zeroMV, &inter_pred_params);

  *derivative = pred2[0] - pred1[0];
}
// Numerical differentiate over window_size x window_size surrounding (x,y)
// location. Alters ix, iy, it to contain numerical partial derivatives
void gradients_over_window(const YV12_BUFFER_CONFIG *frame,
                           const YV12_BUFFER_CONFIG *ref_frame,
                           const double x_coord, const double y_coord,
                           const int window_size, const int bit_depth,
                           double *ix, double *iy, double *it, LOCALMV *mv) {
  const double left = x_coord - window_size / 2;
  const double top = y_coord - window_size / 2;
  // gradient operators need pixel before and after (start at 1)
  const double x_start = AOMMAX(1, left);
  const double y_start = AOMMAX(1, top);
  const int frame_height = frame->y_crop_height;
  const int frame_width = frame->y_crop_width;
  double deriv_x;
  double deriv_y;
  double deriv_t;

  const double x_end = AOMMIN(x_coord + window_size / 2, frame_width - 2);
  const double y_end = AOMMIN(y_coord + window_size / 2, frame_height - 2);
  const int xs = (int)AOMMAX(1, x_start - 1);
  const int ys = (int)AOMMAX(1, y_start - 1);
  const int xe = (int)AOMMIN(x_end + 2, frame_width - 2);
  const int ye = (int)AOMMIN(y_end + 2, frame_height - 2);
  // with normalization, gradients may be double values
  double *fullpel_dx = aom_malloc((ye - ys) * (xe - xs) * sizeof(deriv_x));
  double *fullpel_dy = aom_malloc((ye - ys) * (xe - xs) * sizeof(deriv_y));
  // TODO(any): This could be more efficient in the case that x_coord
  // and y_coord are integers.. but it may look more messy.

  // calculate spatial gradients at full pixel locations
  for (int j = ys; j < ye; j++) {
    for (int i = xs; i < xe; i++) {
      spatial_gradient(frame, i, j, 0, &deriv_x);
      spatial_gradient(frame, i, j, 1, &deriv_y);
      int idx = (j - ys) * (xe - xs) + (i - xs);
      fullpel_dx[idx] = deriv_x;
      fullpel_dy[idx] = deriv_y;
    }
  }
  // compute numerical differentiation for every pixel in window
  // (this potentially includes subpixels)
  for (double j = y_start; j < y_end; j++) {
    for (double i = x_start; i < x_end; i++) {
      temporal_gradient(frame, ref_frame, i, j, bit_depth, &deriv_t, mv);
      gradient_interp(fullpel_dx, i - xs, j - ys, xe - xs, ye - ys, &deriv_x);
      gradient_interp(fullpel_dy, i - xs, j - ys, xe - xs, ye - ys, &deriv_y);
      int idx = (int)(j - top) * window_size + (int)(i - left);
      ix[idx] = deriv_x;
      iy[idx] = deriv_y;
      it[idx] = deriv_t;
    }
  }
  // TODO(any): to avoid setting deriv arrays to zero for every iteration,
  // could instead pass these two values back through function call
  // int first_idx = (int)(y_start - top) * window_size + (int)(x_start - left);
  // int width = window_size - ((int)(x_start - left) + (int)(left + window_size
  // - x_end));

  aom_free(fullpel_dx);
  aom_free(fullpel_dy);
}

// To compute eigenvalues of 2x2 matrix: Solve for lambda where
// Determinant(matrix - lambda*identity) == 0
void eigenvalues_2x2(const double *matrix, double *eig) {
  const double a = 1;
  const double b = -1 * matrix[0] - matrix[3];
  const double c = -1 * matrix[1] * matrix[2] + matrix[0] * matrix[3];
  // quadratic formula
  const double discriminant = b * b - 4 * a * c;
  eig[0] = (-b - sqrt(discriminant)) / (2.0 * a);
  eig[1] = (-b + sqrt(discriminant)) / (2.0 * a);
  // double check that eigenvalues are ordered by magnitude
  if (fabs(eig[0]) > fabs(eig[1])) {
    double tmp = eig[0];
    eig[0] = eig[1];
    eig[1] = tmp;
  }
}
// Shi-Tomasi corner detection criteria
double corner_score(const YV12_BUFFER_CONFIG *frame_to_filter,
                    const YV12_BUFFER_CONFIG *ref_frame, const int x,
                    const int y, double *i_x, double *i_y, double *i_t,
                    const int n, const int bit_depth) {
  double eig[2];
  LOCALMV mv = { .row = 0, .col = 0 };
  // TODO(any): technically, ref_frame and i_t are not used by corner score
  // so these could be replaced by dummy variables,
  // or change this to spatial gradient function over window only
  gradients_over_window(frame_to_filter, ref_frame, x, y, n, bit_depth, i_x,
                        i_y, i_t, &mv);
  double Mres1[1] = { 0 }, Mres2[1] = { 0 }, Mres3[1] = { 0 };
  multiply_mat(i_x, i_x, Mres1, 1, n * n, 1);
  multiply_mat(i_x, i_y, Mres2, 1, n * n, 1);
  multiply_mat(i_y, i_y, Mres3, 1, n * n, 1);
  double M[4] = { Mres1[0], Mres2[0], Mres2[0], Mres3[0] };
  eigenvalues_2x2(M, eig);
  return fabs(eig[0]);
}
// Finds corners in frame_to_filter
// For less strict requirements (i.e. more corners), decrease threshold
int detect_corners(const YV12_BUFFER_CONFIG *frame_to_filter,
                   const YV12_BUFFER_CONFIG *ref_frame, const int maxcorners,
                   int *ref_corners, const int bit_depth) {
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  // TODO(any): currently if maxcorners is decreased, then it only means
  // corners will be omited from bottom-right of image. if maxcorners
  // is actually used, then this algorithm would need to re-iterate
  // and choose threshold based on that
  assert(maxcorners == frame_height * frame_width);
  int countcorners = 0;
  const double threshold = 0.1;
  double score;
  const int n = 3;
  double i_x[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  double i_y[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  double i_t[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  const int fromedge = n;
  double max_score = corner_score(frame_to_filter, ref_frame, fromedge,
                                  fromedge, i_x, i_y, i_t, n, bit_depth);
  // rough estimate of max corner score in image
  for (int x = fromedge; x < frame_width - fromedge; x += 1) {
    for (int y = fromedge; y < frame_height - fromedge; y += frame_height / 5) {
      for (int i = 0; i < n * n; i++) {
        i_x[i] = 0;
        i_y[i] = 0;
        i_t[i] = 0;
      }
      score = corner_score(frame_to_filter, ref_frame, x, y, i_x, i_y, i_t, n,
                           bit_depth);
      if (score > max_score) {
        max_score = score;
      }
    }
  }
  // score all the points and choose corners over threshold
  for (int x = fromedge; x < frame_width - fromedge; x += 1) {
    for (int y = fromedge;
         (y < frame_height - fromedge) && countcorners < maxcorners; y += 1) {
      for (int i = 0; i < n * n; i++) {
        i_x[i] = 0;
        i_y[i] = 0;
        i_t[i] = 0;
      }
      score = corner_score(frame_to_filter, ref_frame, x, y, i_x, i_y, i_t, n,
                           bit_depth);
      if (score > threshold * max_score) {
        ref_corners[countcorners * 2] = x;
        ref_corners[countcorners * 2 + 1] = y;
        countcorners++;
      }
    }
  }
  return countcorners;
}
// weights is an nxn matrix. weights is filled with a gaussian function,
// with independent variable: distance from the center point.
void gaussian(const double sigma, const int n, const int normalize,
              double *weights) {
  double total_weight = 0;
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      double distance = sqrt(pow(n / 2 - i, 2) + pow(n / 2 - j, 2));
      double weight = exp(-0.5 * pow(distance / sigma, 2));
      weights[j * n + i] = weight;
      total_weight += weight;
    }
  }
  if (normalize == 1) {
    for (int j = 0; j < n; j++) {
      weights[j] = weights[j] / total_weight;
    }
  }
}
double convolve(const double *filter, const int *img, const int size) {
  double result = 0;
  for (int i = 0; i < size; i++) {
    result += filter[i] * img[i];
  }
  return result;
}
// Applies a Gaussian low-pass smoothing filter to produce
// a corresponding lower resolution image with halved dimensions
void reduce(uint8_t *img, int height, int width, int stride,
            uint8_t *reduced_img) {
  const int new_width = width / 2;
  const int window_size = 5;
  const double gaussian_filter[25] = {
    1. / 256, 1.0 / 64, 3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };
  // filter is 5x5 so need prev and forward 2 pixels
  int img_section[25];
  for (int y = 0; y < height - 1; y += 2) {
    for (int x = 0; x < width - 1; x += 2) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = yy;
          int xvalue = xx;
          // copied pixels outside the boundary
          if (yvalue < 0) yvalue = 0;
          if (xvalue < 0) xvalue = 0;
          if (yvalue >= height) yvalue = height - 1;
          if (xvalue >= width) xvalue = width - 1;
          img_section[i++] = img[yvalue * stride + xvalue];
        }
      }
      reduced_img[(y / 2) * new_width + (x / 2)] = (uint8_t)convolve(
          gaussian_filter, img_section, (int)pow(window_size, 2));
    }
  }
}
int cmpfunc(const void *a, const void *b) { return (*(int *)a - *(int *)b); }
void filter_mvs(const MV_FILTER_TYPE mv_filter, const int frame_height,
                const int frame_width, LOCALMV *localmvs, MV *mvs) {
  const int n = 5;  // window size
  // for smoothing filter
  const double gaussian_filter[25] = {
    1. / 256, 1. / 64,  3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };
  // for median filter
  int mvrows[25];
  int mvcols[25];
  if (mv_filter != MV_FILTER_NONE) {
    for (int y = 0; y < frame_height; y++) {
      for (int x = 0; x < frame_width; x++) {
        int center_idx = y * frame_width + x;
        if (fabs(localmvs[center_idx].row) > 0 ||
            fabs(localmvs[center_idx].col) > 0) {
          int i = 0;
          double filtered_row = 0;
          double filtered_col = 0;
          for (int yy = y - n / 2; yy <= y + n / 2; yy++) {
            for (int xx = x - n / 2; xx <= x + n / 2; xx++) {
              int yvalue = yy + y;
              int xvalue = xx + x;
              // copied pixels outside the boundary
              if (yvalue < 0) yvalue = 0;
              if (xvalue < 0) xvalue = 0;
              if (yvalue >= frame_height) yvalue = frame_height - 1;
              if (xvalue >= frame_width) xvalue = frame_width - 1;
              int index = yvalue * frame_width + xvalue;
              if (mv_filter == MV_FILTER_SMOOTH) {
                filtered_row += mvs[index].row * gaussian_filter[i];
                filtered_col += mvs[index].col * gaussian_filter[i];
              } else if (mv_filter == MV_FILTER_MEDIAN) {
                mvrows[i] = mvs[index].row;
                mvcols[i] = mvs[index].col;
              }
              i++;
            }
          }

          MV mv = mvs[center_idx];
          if (mv_filter == MV_FILTER_SMOOTH) {
            mv.row = (int16_t)filtered_row;
            mv.col = (int16_t)filtered_col;
          } else if (mv_filter == MV_FILTER_MEDIAN) {
            qsort(mvrows, 25, sizeof(mv.row), cmpfunc);
            qsort(mvcols, 25, sizeof(mv.col), cmpfunc);
            mv.row = mvrows[25 / 2];
            mv.col = mvcols[25 / 2];
          }
          LOCALMV localmv = { .row = ((double)mv.row) / 8,
                              .col = ((double)mv.row) / 8 };
          localmvs[y * frame_width + x] = localmv;
          // if mvs array is immediately updated here, then the result may
          // propagate to other pixels.
        }
      }
    }
    for (int i = 0; i < frame_height * frame_width; i++) {
      if (fabs(localmvs[i].row) > 0 || fabs(localmvs[i].col) > 0) {
        MV mv = { .row = (int16_t)round(8 * localmvs[i].row),
                  .col = (int16_t)round(8 * localmvs[i].col) };
        mvs[i] = mv;
      }
    }
  }
}

// Computes optical flow at a single pyramid level,
// using Lucas-Kanade algorithm.
// Modifies mvs array.
void lucas_kanade(const YV12_BUFFER_CONFIG *frame_to_filter,
                  const YV12_BUFFER_CONFIG *ref_frame, const int level,
                  const LK_PARAMS *lk_params, const int num_ref_corners,
                  int *ref_corners, const int highres_frame_width,
                  const int bit_depth, LOCALMV *mvs) {
  assert(lk_params->window_size > 0 && lk_params->window_size % 2 == 0);
  const int n = lk_params->window_size;
  // algorithm is sensitive to window size
  double *i_x = (double *)aom_malloc(n * n * sizeof(double));
  double *i_y = (double *)aom_malloc(n * n * sizeof(double));
  double *i_t = (double *)aom_malloc(n * n * sizeof(double));
  const int expand_multiplier = (int)pow(2, level);
  double sigma = 0.2 * n;
  double *weights = (double *)aom_malloc(n * n * sizeof(double));
  // normalizing doesn't really affect anything since it's applied
  // to every component of M and b
  gaussian(sigma, n, 0, weights);
  for (int i = 0; i < num_ref_corners; i++) {
    const double x_coord = 1.0 * ref_corners[i * 2] / expand_multiplier;
    const double y_coord = 1.0 * ref_corners[i * 2 + 1] / expand_multiplier;
    int highres_x = ref_corners[i * 2];
    int highres_y = ref_corners[i * 2 + 1];
    int mv_idx = highres_y * (highres_frame_width) + highres_x;
    LOCALMV mv_old = mvs[mv_idx];
    mv_old.row = mv_old.row / expand_multiplier;
    mv_old.col = mv_old.col / expand_multiplier;
    // using this instead of memset, since it's not completely
    // clear if zero memset works on double arrays
    for (int j = 0; j < n * n; j++) {
      i_x[j] = 0;
      i_y[j] = 0;
      i_t[j] = 0;
    }
    gradients_over_window(frame_to_filter, ref_frame, x_coord, y_coord, n,
                          bit_depth, i_x, i_y, i_t, &mv_old);
    double Mres1[1] = { 0 }, Mres2[1] = { 0 }, Mres3[1] = { 0 };
    double bres1[1] = { 0 }, bres2[1] = { 0 };
    for (int j = 0; j < n * n; j++) {
      Mres1[0] += weights[j] * i_x[j] * i_x[j];
      Mres2[0] += weights[j] * i_x[j] * i_y[j];
      Mres3[0] += weights[j] * i_y[j] * i_y[j];
      bres1[0] += weights[j] * i_x[j] * i_t[j];
      bres2[0] += weights[j] * i_y[j] * i_t[j];
    }
    double M[4] = { Mres1[0], Mres2[0], Mres2[0], Mres3[0] };
    double b[2] = { -1 * bres1[0], -1 * bres2[0] };
    double eig[2] = { 1, 1 };
    eigenvalues_2x2(M, eig);
    double threshold = 0.1;
    if (fabs(eig[0]) > threshold) {
      // if M is not invertible, then displacement
      // will default to zeros
      double u[2] = { 0, 0 };
      linsolve(2, M, 2, b, u);
      int mult = 1;
      if (level != 0)
        mult = expand_multiplier;  // mv doubles when resolution doubles
      LOCALMV mv = { .row = (mult * (u[0] + mv_old.row)),
                     .col = (mult * (u[1] + mv_old.col)) };
      mvs[mv_idx] = mv;
      mvs[mv_idx] = mv;
    }
  }
  aom_free(weights);
  aom_free(i_t);
  aom_free(i_x);
  aom_free(i_y);
}

// Apply optical flow iteratively at each pyramid level
void pyramid_optical_flow(const YV12_BUFFER_CONFIG *from_frame,
                          const YV12_BUFFER_CONFIG *to_frame,
                          const int bit_depth, const OPFL_PARAMS *opfl_params,
                          const OPTFLOW_METHOD method, LOCALMV *mvs) {
  assert(opfl_params->pyramid_levels > 0 &&
         opfl_params->pyramid_levels <= MAX_PYRAMID_LEVELS);
  int levels = opfl_params->pyramid_levels;
  const int frame_height = from_frame->y_crop_height;
  const int frame_width = from_frame->y_crop_width;
  if ((frame_height / pow(2.0, levels - 1) < 50 ||
       frame_height / pow(2.0, levels - 1) < 50) &&
      levels > 1)
    levels = levels - 1;
  uint8_t *images1[MAX_PYRAMID_LEVELS];
  uint8_t *images2[MAX_PYRAMID_LEVELS];
  images1[0] = from_frame->y_buffer;
  images2[0] = to_frame->y_buffer;
  YV12_BUFFER_CONFIG *buffers1 =
      aom_malloc(levels * sizeof(YV12_BUFFER_CONFIG));
  YV12_BUFFER_CONFIG *buffers2 =
      aom_malloc(levels * sizeof(YV12_BUFFER_CONFIG));
  buffers1[0] = *from_frame;
  buffers2[0] = *to_frame;
  int fw = frame_width;
  int fh = frame_height;
  for (int i = 1; i < levels; i++) {
    images1[i] = (uint8_t *)aom_calloc(fh / 2 * fw / 2, sizeof(uint8_t));
    images2[i] = (uint8_t *)aom_calloc(fh / 2 * fw / 2, sizeof(uint8_t));
    int stride;
    if (i == 1)
      stride = from_frame->y_stride;
    else
      stride = fw;
    reduce(images1[i - 1], fh, fw, stride, images1[i]);
    reduce(images2[i - 1], fh, fw, stride, images2[i]);
    fh /= 2;
    fw /= 2;
    YV12_BUFFER_CONFIG a = { .y_buffer = images1[i],
                             .y_crop_width = fw,
                             .y_crop_height = fh,
                             .y_stride = fw };
    YV12_BUFFER_CONFIG b = { .y_buffer = images2[i],
                             .y_crop_width = fw,
                             .y_crop_height = fh,
                             .y_stride = fw };
    buffers1[i] = a;
    buffers2[i] = b;
  }
  // Compute corners for specific frame
  int maxcorners = from_frame->y_crop_width * from_frame->y_crop_height;
  int *ref_corners = aom_malloc(maxcorners * 2 * sizeof(int));
  int num_ref_corners = 0;
  if (is_sparse(opfl_params)) {
    num_ref_corners = detect_corners(from_frame, to_frame, maxcorners,
                                     ref_corners, bit_depth);
  }
  const int stop_level = 0;
  for (int i = levels - 1; i >= stop_level; i--) {
    if (method == LUCAS_KANADE) {
      assert(is_sparse(opfl_params));
      lucas_kanade(&buffers1[i], &buffers2[i], i, opfl_params->lk_params,
                   num_ref_corners, ref_corners, buffers1[0].y_crop_width,
                   bit_depth, mvs);
    }
  }
  for (int i = 1; i < levels; i++) {
    aom_free(images1[i]);
    aom_free(images2[i]);
  }
  aom_free(ref_corners);
}
// Computes optical flow by applying algorithm at
// multiple pyramid levels of images (lower-resolution, smoothed images)
// This accounts for larger motions.
// Inputs:
//   from_frame Frame buffer.
//   to_frame: Frame buffer. MVs point from_frame -> to_frame.
//   from_frame_idx: Index of from_frame.
//   to_frame_idx: Index of to_frame. Return all zero MVs when idx are equal.
//   bit_depth:
//   opfl_params: contains algorithm-specific parameters.
//   mv_filter: MV_FILTER_NONE, MV_FILTER_SMOOTH, or MV_FILTER_MEDIAN.
//   method: LUCAS_KANADE,
//   mvs: pointer to MVs. Contains initialization, and modified
//   based on optical flow. Must have
//   dimensions = from_frame->y_crop_width * from_frame->y_crop_height
void optical_flow(const YV12_BUFFER_CONFIG *from_frame,
                  const YV12_BUFFER_CONFIG *to_frame, const int from_frame_idx,
                  const int to_frame_idx, const int bit_depth,
                  const OPFL_PARAMS *opfl_params,
                  const MV_FILTER_TYPE mv_filter, const OPTFLOW_METHOD method,
                  MV *mvs) {
  const int frame_height = from_frame->y_crop_height;
  const int frame_width = from_frame->y_crop_width;
  // TODO(any): deal with the case where frames are not of the same dimensions
  assert(frame_height == to_frame->y_crop_height &&
         frame_width == to_frame->y_crop_width);
  if (from_frame_idx == to_frame_idx) {
    // immediately return all zero mvs when frame indices are equal
    for (int yy = 0; yy < frame_height; yy++) {
      for (int xx = 0; xx < frame_width; xx++) {
        MV mv = { .row = 0, .col = 0 };
        mvs[yy * frame_width + xx] = mv;
      }
    }
    return;
  }

  // Initialize double mvs based on input parameter mvs array
  LOCALMV *localmvs = aom_malloc(frame_height * frame_width * sizeof(LOCALMV));
  for (int i = 0; i < frame_width * frame_height; i++) {
    MV mv = mvs[i];
    LOCALMV localmv = { .row = ((double)mv.row) / 8,
                        .col = ((double)mv.col) / 8 };
    localmvs[i] = localmv;
  }
  // Apply optical flow algorithm
  pyramid_optical_flow(from_frame, to_frame, bit_depth, opfl_params, method,
                       localmvs);

  // Update original mvs array
  for (int j = 0; j < frame_height; j++) {
    for (int i = 0; i < frame_width; i++) {
      int idx = j * frame_width + i;
      int new_x = (int)(localmvs[idx].row + i);
      int new_y = (int)(localmvs[idx].col + j);
      if ((fabs(localmvs[idx].row) >= 0.125 ||
           fabs(localmvs[idx].col) >= 0.125)) {
        // if mv points outside of frame (lost feature), keep old mv.
        if (new_x < frame_width && new_x >= 0 && new_y < frame_height &&
            new_y >= 0) {
          MV mv = { .row = (int16_t)round(8 * localmvs[idx].row),
                    .col = (int16_t)round(8 * localmvs[idx].col) };
          mvs[idx] = mv;
        }
      }
    }
  }

  filter_mvs(mv_filter, frame_height, frame_width, localmvs, mvs);

  aom_free(localmvs);
}
#endif
