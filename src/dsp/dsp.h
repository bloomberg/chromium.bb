/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_DSP_DSP_H_
#define LIBGAV1_SRC_DSP_DSP_H_

#include <cstddef>  // ptrdiff_t
#include <cstdint>
#include <cstdlib>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/utils/cpu.h"

namespace libgav1 {
namespace dsp {

#if !defined(LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS)
#define LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS 0
#endif

enum IntraPredictor : uint8_t {
  kIntraPredictorDcFill,
  kIntraPredictorDcTop,
  kIntraPredictorDcLeft,
  kIntraPredictorDc,
  kIntraPredictorVertical,
  kIntraPredictorHorizontal,
  kIntraPredictorPaeth,
  kIntraPredictorSmooth,
  kIntraPredictorSmoothVertical,
  kIntraPredictorSmoothHorizontal,
  kNumIntraPredictors
};

// List of valid 1D transforms.
enum Transform1D : uint8_t {
  k1DTransformDct,   // Discrete Cosine Transform.
  k1DTransformAdst,  // Asymmetric Discrete Sine Transform.
  k1DTransformIdentity,
  k1DTransformWht,  // Walsh Hadamard Transform.
  kNum1DTransforms
};

// List of valid 1D transform sizes. Not all transforms may be available for all
// the sizes.
enum TransformSize1D : uint8_t {
  k1DTransformSize4,
  k1DTransformSize8,
  k1DTransformSize16,
  k1DTransformSize32,
  k1DTransformSize64,
  kNum1DTransformSizes
};

// The maximum width of the loop filter, fewer pixels may be filtered depending
// on strength thresholds.
enum LoopFilterSize : uint8_t {
  kLoopFilterSize4,
  kLoopFilterSize6,
  kLoopFilterSize8,
  kLoopFilterSize14,
  kNumLoopFilterSizes
};

//------------------------------------------------------------------------------
// ToString()
//
// These functions are meant to be used only in debug logging and within tests.
// They are defined inline to avoid including the strings in the release
// library when logging is disabled; unreferenced functions will not be added to
// any object file in that case.

inline const char* ToString(const IntraPredictor predictor) {
  switch (predictor) {
    case kIntraPredictorDcFill:
      return "kIntraPredictorDcFill";
    case kIntraPredictorDcTop:
      return "kIntraPredictorDcTop";
    case kIntraPredictorDcLeft:
      return "kIntraPredictorDcLeft";
    case kIntraPredictorDc:
      return "kIntraPredictorDc";
    case kIntraPredictorVertical:
      return "kIntraPredictorVertical";
    case kIntraPredictorHorizontal:
      return "kIntraPredictorHorizontal";
    case kIntraPredictorPaeth:
      return "kIntraPredictorPaeth";
    case kIntraPredictorSmooth:
      return "kIntraPredictorSmooth";
    case kIntraPredictorSmoothVertical:
      return "kIntraPredictorSmoothVertical";
    case kIntraPredictorSmoothHorizontal:
      return "kIntraPredictorSmoothHorizontal";
    case kNumIntraPredictors:
      return "kNumIntraPredictors";
  }
  abort();
}

inline const char* ToString(const Transform1D transform) {
  switch (transform) {
    case k1DTransformDct:
      return "k1DTransformDct";
    case k1DTransformAdst:
      return "k1DTransformAdst";
    case k1DTransformIdentity:
      return "k1DTransformIdentity";
    case k1DTransformWht:
      return "k1DTransformWht";
    case kNum1DTransforms:
      return "kNum1DTransforms";
  }
  abort();
}

inline const char* ToString(const TransformSize1D transform_size) {
  switch (transform_size) {
    case k1DTransformSize4:
      return "k1DTransformSize4";
    case k1DTransformSize8:
      return "k1DTransformSize8";
    case k1DTransformSize16:
      return "k1DTransformSize16";
    case k1DTransformSize32:
      return "k1DTransformSize32";
    case k1DTransformSize64:
      return "k1DTransformSize64";
    case kNum1DTransformSizes:
      return "kNum1DTransformSizes";
  }
  abort();
}

inline const char* ToString(const LoopFilterSize filter_size) {
  switch (filter_size) {
    case kLoopFilterSize4:
      return "kLoopFilterSize4";
    case kLoopFilterSize6:
      return "kLoopFilterSize6";
    case kLoopFilterSize8:
      return "kLoopFilterSize8";
    case kLoopFilterSize14:
      return "kLoopFilterSize14";
    case kNumLoopFilterSizes:
      return "kNumLoopFilterSizes";
  }
  abort();
}

inline const char* ToString(const LoopFilterType filter_type) {
  switch (filter_type) {
    case kLoopFilterTypeVertical:
      return "kLoopFilterTypeVertical";
    case kLoopFilterTypeHorizontal:
      return "kLoopFilterTypeHorizontal";
    case kNumLoopFilterTypes:
      return "kNumLoopFilterTypes";
  }
  abort();
}

//------------------------------------------------------------------------------
// Intra predictors. Section 7.11.2.
// These require access to one or both of the top row and left column. Some may
// access the top-left (top[-1]), top-right (top[width+N]), bottom-left
// (left[height+N]) or upper-left (left[-1]).

// Intra predictor function signature. Sections 7.11.2.2, 7.11.2.4 (#10,#11),
// 7.11.2.5, 7.11.2.6.
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes. |top| is an unaligned pointer to
// the row above |dst|. |left| is an aligned vector of the column to the left
// of |dst|. top-left and bottom-left may be accessed.
using IntraPredictorFunc = void (*)(void* dst, ptrdiff_t stride,
                                    const void* top, const void* left);
using IntraPredictorFuncs =
    IntraPredictorFunc[kNumTransformSizes][kNumIntraPredictors];

// Directional intra predictor function signature, zone 1 (0 < angle < 90).
// Section 7.11.2.4 (#7).
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes. |top| is an unaligned pointer to
// the row above |dst|. |width| and |height| give the dimensions of the block.
// |xstep| is the scaled starting index to |top| from
// kDirectionalIntraPredictorDerivative. |upsampled_top| indicates whether
// |top| has been upsampled as described in '7.11.2.11. Intra edge upsample
// process'. This can occur in cases with |width| + |height| <= 16. top-right
// is accessed.
using DirectionalIntraPredictorZone1Func = void (*)(void* dst, ptrdiff_t stride,
                                                    const void* top, int width,
                                                    int height, int xstep,
                                                    bool upsampled_top);

// Directional intra predictor function signature, zone 2 (90 < angle < 180).
// Section 7.11.2.4 (#8).
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes. |top| is an unaligned pointer to
// the row above |dst|. |left| is an aligned vector of the column to the left of
// |dst|. |width| and |height| give the dimensions of the block. |xstep| and
// |ystep| are the scaled starting index to |top| and |left|, respectively,
// from kDirectionalIntraPredictorDerivative. |upsampled_top| and
// |upsampled_left| indicate whether |top| and |left| have been upsampled as
// described in '7.11.2.11. Intra edge upsample process'. This can occur in
// cases with |width| + |height| <= 16. top-left and upper-left are accessed,
// up to [-2] in each if |upsampled_top/left| are set.
using DirectionalIntraPredictorZone2Func = void (*)(
    void* dst, ptrdiff_t stride, const void* top, const void* left, int width,
    int height, int xstep, int ystep, bool upsampled_top, bool upsampled_left);

// Directional intra predictor function signature, zone 3 (180 < angle < 270).
// Section 7.11.2.4 (#9).
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes. |left| is an aligned vector of the
// column to the left of |dst|. |width| and |height| give the dimensions of the
// block. |ystep| is the scaled starting index to |left| from
// kDirectionalIntraPredictorDerivative. |upsampled_left| indicates whether
// |left| has been upsampled as described in '7.11.2.11. Intra edge upsample
// process'. This can occur in cases with |width| + |height| <= 16. bottom-left
// is accessed.
using DirectionalIntraPredictorZone3Func = void (*)(void* dst, ptrdiff_t stride,
                                                    const void* left, int width,
                                                    int height, int ystep,
                                                    bool upsampled_left);

// Filter intra predictor function signature. Section 7.11.2.3.
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes. |top| is an unaligned pointer to
// the row above |dst|. |left| is an aligned vector of the column to the left
// of |dst|. |width| and |height| are the size of the block in pixels.
using FilterIntraPredictorFunc = void (*)(void* dst, ptrdiff_t stride,
                                          const void* top, const void* left,
                                          FilterIntraPredictor pred, int width,
                                          int height);

//------------------------------------------------------------------------------
// Chroma from Luma (Cfl) prediction. Section 7.11.5.

// Chroma from Luma (Cfl) intra prediction function signature. |dst| is an
// unaligned pointer to the output block. Pixel size is determined by bitdepth
// with |stride| given in bytes. |luma| contains subsampled luma pixels with 3
// fractional bits of precision. |alpha| is the signed Cfl alpha value for the
// appropriate plane.
using CflIntraPredictorFunc = void (*)(
    void* dst, ptrdiff_t stride,
    const int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride], int alpha);
using CflIntraPredictorFuncs = CflIntraPredictorFunc[kNumTransformSizes];

// Chroma from Luma (Cfl) subsampler function signature. |luma| is an unaligned
// pointer to the output block. |src| is an unaligned pointer to the input
// block. Pixel size is determined by bitdepth with |stride| given in bytes.
using CflSubsamplerFunc =
    void (*)(int16_t luma[kCflLumaBufferStride][kCflLumaBufferStride],
             int max_luma_width, int max_luma_height, const void* source,
             ptrdiff_t stride);
using CflSubsamplerFuncs =
    CflSubsamplerFunc[kNumTransformSizes][kNumSubsamplingTypes];

//------------------------------------------------------------------------------
// Intra Edge Filtering and Upsampling. Step 4 in section 7.11.2.4.

// Intra edge filter function signature. |buffer| is a pointer to the top_row or
// left_column that needs to be filtered. Typically the -1'th index of |top_row|
// and |left_column| need to be filtered as well, so the caller can merely pass
// the |buffer| as top_row[-1] or left_column[-1]. Pixel size is determined by
// bitdepth. |size| is the number of pixels to be filtered. |strength| is the
// filter strength. Section 7.11.2.12 in the spec.
using IntraEdgeFilterFunc = void (*)(void* buffer, int size, int strength);

// Intra edge upsampler function signature. |buffer| is a pointer to the top_row
// or left_column that needs to be upsampled. Pixel size is determined by
// bitdepth. |size| is the number of pixels to be upsampled; valid values are:
// 4, 8, 12, 16. This function needs access to negative indices -1 and -2 of
// the |buffer|. Section 7.11.2.11 in the spec.
using IntraEdgeUpsamplerFunc = void (*)(void* buffer, int size);

//------------------------------------------------------------------------------
// Inverse transform add function signature.
//
// Steps 2 and 3 of section 7.12.3 (contains the implementation of section
// 7.13.3).
// Apply the inverse transforms and add the residual to the destination frame
// for the transform type and block size |tx_size| starting at position
// |start_x| and |start_y|.  |dst_frame| is a pointer to an Array2D. |is_row|
// signals the direction of the transform loop. |non_zero_coeff_count| is the
// number of non zero coefficients in the block.
using InverseTransformAddFunc = void (*)(TransformType tx_type,
                                         TransformSize tx_size,
                                         void* src_buffer, int start_x,
                                         int start_y, void* dst_frame,
                                         bool is_row, int non_zero_coeff_count);
using InverseTransformAddFuncs =
    InverseTransformAddFunc[kNum1DTransformSizes][kNum1DTransforms];

//------------------------------------------------------------------------------
// Post processing.

// Loop filter function signature. Section 7.14.
// |dst| is an unaligned pointer to the output block. Pixel size is determined
// by bitdepth with |stride| given in bytes.
using LoopFilterFunc = void (*)(void* dst, ptrdiff_t stride, int outer_thresh,
                                int inner_thresh, int hev_thresh);
using LoopFilterFuncs =
    LoopFilterFunc[kNumLoopFilterSizes][kNumLoopFilterTypes];

// Cdef direction function signature. Section 7.15.2.
// |src| is a pointer to the source block. Pixel size is determined by bitdepth
// with |stride| given in bytes. |direction| and |variance| are output
// parameters and must not be nullptr.
using CdefDirectionFunc = void (*)(const void* src, ptrdiff_t stride,
                                   int* direction, int* variance);

// Cdef filtering function signature. Section 7.15.3.
// |source| is a pointer to the input block. |source_stride| is given in bytes.
// |rows4x4| and |columns4x4| are frame sizes in units of 4x4 pixels.
// |curr_x| and |curr_y| are current position in units of pixels.
// |subsampling_x|, |subsampling_y| are the subsampling factors of current
// plane.
// |primary_strength|, |secondary_strength|, and |damping| are Cdef filtering
// parameters.
// |direction| is the filtering direction.
// |dest| is the output buffer. |dest_stride| is given in bytes.
using CdefFilteringFunc = void (*)(const void* source, ptrdiff_t source_stride,
                                   int rows4x4, int columns4x4, int curr_x,
                                   int curr_y, int subsampling_x,
                                   int subsampling_y, int primary_strength,
                                   int secondary_strength, int damping,
                                   int direction, void* dest,
                                   ptrdiff_t dest_stride);

// Loop restoration function signature. Sections 7.16, 7.17.
// |source| is the input frame buffer, which is deblocked and cdef filtered.
// |dest| is the output.
// |restoration_info| contains loop restoration information, such as filter
// type, strength. |source| and |dest| share the same stride given in bytes.
// |buffer| contains buffers required for self guided filter and wiener filter.
// They must be initialized before calling.
using LoopRestorationFunc = void (*)(
    const void* source, void* dest, const RestorationUnitInfo& restoration_info,
    ptrdiff_t source_stride, ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* buffer);

// Index 0 is Wiener Filter.
// Index 1 is Self Guided Restoration Filter.
// This can be accessed as LoopRestorationType - 2.
using LoopRestorationFuncs = LoopRestorationFunc[2];

// Convolve function signature. Section 7.11.3.4.
// This function applies a horizontal filter followed by a vertical filter.
// |reference| is the input block (reference frame buffer). |reference_stride|
// is the corresponding frame stride.
// |vertical_filter_index|/|horizontal_filter_index| is the index to
// retrieve the type of filter to be applied for vertical/horizontal direction
// from the filter lookup table 'kSubPixelFilters'.
// |inter_round_bits_vertical| is the rounding precision used after vertical
// filtering (7 or 11). kInterRoundBitsHorizontal &
// kInterRoundBitsHorizontal12bpp can be used after the horizontal pass.
// |subpixel_x| and |subpixel_y| are starting positions in units of 1/1024.
// |step_x| and |step_y| are step sizes in units of 1/1024 of a pixel.
// |width| and |height| are width and height of the block to be filtered.
// |ref_last_x| and |ref_last_y| are the last pixel of the reference frame in
// x/y direction.
// |prediction| is the output block (output frame buffer).
using ConvolveFunc = void (*)(const void* reference, ptrdiff_t reference_stride,
                              int horizontal_filter_index,
                              int vertical_filter_index,
                              int inter_round_bits_vertical, int subpixel_x,
                              int subpixel_y, int step_x, int step_y, int width,
                              int height, void* prediction,
                              ptrdiff_t pred_stride);

// Convolve functions signature. Each points to one convolve function with
// a specific setting:
// ConvolveFunc[is_intra_block_copy][is_compound][has_vertical_filter]
// [has_horizontal_filter].
// If is_compound is false, the prediction is clipped to pixel.
// If is_compound is true, the range of prediction is:
//   8bpp: [0, 15471]
//   10bpp: [0, 61983]
//   12bpp: [0, 62007]
using ConvolveFuncs = ConvolveFunc[2][2][2][2];

// Convolve functions signature for scaling version.
// 0: single predictor. 1: compound predictor.
using ConvolveScaleFuncs = ConvolveFunc[2];

// Weight mask function signature. Section 7.11.3.12.
// |prediction_0| is the first input block.
// |prediction_1| is the second input block.
// |prediction_stride_0| and |prediction_stride_1| are corresponding strides.
// |width| and |height| are the prediction width and height.
// |mask| is the output buffer. |mask_stride| is the output buffer stride.
using WeightMaskFunc = void (*)(const uint16_t* prediction_0,
                                ptrdiff_t stride_0,
                                const uint16_t* prediction_1,
                                ptrdiff_t stride_1, uint8_t* mask,
                                ptrdiff_t mask_stride);

// Weight mask functions signature. The dimensions (in order) are:
//   * Width index (4 => 0, 8 => 1, 16 => 2 and so on).
//   * Height index (4 => 0, 8 => 1, 16 => 2 and so on).
//   * Bitdepth index (8 => 0, 10 => 1, 12 => 2).
//   * mask_is_inverse.
using WeightMaskFuncs = WeightMaskFunc[6][6][3][2];

// Average blending function signature.
// Two predictors are averaged to generate the output.
// Input predictor values are int16_t. Output type is uint8_t, with actual
// range of Pixel value.
// Average blending is in the bottom of Section 7.11.3.1 (COMPOUND_AVERAGE).
// |prediction_0| is the first input block.
// |prediction_1| is the second input block.
// |prediction_stride_0| and |prediction_stride_1| are corresponding strides.
// |width| and |height| are the same for the first and second input blocks.
// The valid range of block size is [8x8, 128x128] for the luma plane.
// |dest| is the output buffer. |dest_stride| is the output buffer stride.
using AverageBlendFunc = void (*)(const uint16_t* prediction_0,
                                  ptrdiff_t prediction_stride_0,
                                  const uint16_t* prediction_1,
                                  ptrdiff_t prediction_stride_1, int width,
                                  int height, void* dest,
                                  ptrdiff_t dest_stride);

// Distance weighted blending function signature.
// Weights are generated in Section 7.11.3.15.
// Weighted blending is in the bottom of Section 7.11.3.1 (COMPOUND_DISTANCE).
// This function takes two blocks (inter frame prediction) and produces a
// weighted output.
// |prediction_0| is the first input block.
// |prediction_1| is the second input block.
// |prediction_stride_0| and |prediction_stride_1| are corresponding strides.
// |weight_0| is the weight for the first block. It is derived from the relative
// distance of the first reference frame and the current frame.
// |weight_1| is the weight for the second block. It is derived from the
// relative distance of the second reference frame and the current frame.
// |width| and |height| are the same for the first and second input blocks.
// The valid range of block size is [8x8, 128x128] for the luma plane.
// |dest| is the output buffer. |dest_stride| is the output buffer stride.
using DistanceWeightedBlendFunc = void (*)(const uint16_t* prediction_0,
                                           ptrdiff_t prediction_stride_0,
                                           const uint16_t* prediction_1,
                                           ptrdiff_t prediction_stride_1,
                                           uint8_t weight_0, uint8_t weight_1,
                                           int width, int height, void* dest,
                                           ptrdiff_t dest_stride);

// Mask blending function signature. Section 7.11.3.14.
// This function takes two blocks and produces a blended output stored into the
// output block |dest|. The blending is a weighted average process, controlled
// by values of the mask.
// |prediction_0| is the first input block. When prediction mode is inter_intra
// (or wedge_inter_intra), this refers to the inter frame prediction.
// |prediction_stride_0| is the stride, given in units of uint16_t.
// |prediction_1| is the second input block. When prediction mode is inter_intra
// (or wedge_inter_intra), this refers to the intra frame prediction.
// |prediction_stride_1| is the stride, given in units of uint16_t.
// |mask| is an integer array, whose value indicates the weight of the blending.
// |mask_stride| is corresponding stride.
// |width|, |height| are the same for both input blocks.
// If it's inter_intra (or wedge_inter_intra), the valid range of block size is
// [8x8, 32x32]. Otherwise (including difference weighted prediction and
// compound average prediction), the valid range is [8x8, 128x128].
// If there's subsampling, the corresponding width and height are halved for
// chroma planes.
// |subsampling_x|, |subsampling_y| are the subsampling factors.
// |is_inter_intra| stands for the prediction mode. If it is true, one of the
// prediction blocks is from intra prediction of current frame. Otherwise, two
// prediction blocks are both inter frame predictions.
// |is_wedge_inter_intra| indicates if the mask is for the wedge prediction.
// |dest| is the output block.
// |dest_stride| is the corresponding stride for dest.
using MaskBlendFunc = void (*)(const uint16_t* prediction_0,
                               ptrdiff_t prediction_stride_0,
                               const uint16_t* prediction_1,
                               ptrdiff_t prediction_stride_1,
                               const uint8_t* mask, ptrdiff_t mask_stride,
                               int width, int height, void* dest,
                               ptrdiff_t dest_stride);

// Mask blending functions signature. Each points to one function with
// a specific setting:
// MaskBlendFunc[subsampling_x + subsampling_y][is_inter_intra].
using MaskBlendFuncs = MaskBlendFunc[3][2];

// This function is similar to the MaskBlendFunc with the only difference that
// |prediction_1| is of type uint8_t instead of uint16_t. This function is used
// only when bitdepth is 8 and is_inter_intra is true.
using InterIntraMaskBlendFunc8bpp =
    void (*)(const uint16_t* prediction_0, ptrdiff_t prediction_stride_0,
             const uint8_t* prediction_1, ptrdiff_t prediction_stride_1,
             const uint8_t* mask, ptrdiff_t mask_stride, int width, int height,
             void* dest, ptrdiff_t dest_stride);

// InterIntra8bpp mask blending functions signature. When is_wedge_inter_intra
// is false, the function at index 0 must be used. Otherwise, the function at
// index subsampling_x + subsampling_y must be used.
using InterIntraMaskBlendFuncs8bpp = InterIntraMaskBlendFunc8bpp[3];

// Obmc (overlapped block motion compensation) blending function signature.
// Section 7.11.3.10.
// This function takes two blocks and produces a blended output stored into the
// first input block. The blending is a weighted average process, controlled by
// values of the mask.
// Obmc is not a compound mode. It is different from other compound blending,
// in terms of precision. The current block is computed using convolution with
// clipping to the range of pixel values. Its above and left blocks are also
// clipped. Therefore obmc blending process doesn't need to clip the output.
// |prediction| is the first input block, which will be overwritten.
// |prediction_stride| is the stride, given in bytes.
// |width|, |height| are the same for both input blocks.
// |obmc_prediction| is the second input block.
// |obmc_prediction_stride| is its stride, given in bytes.
using ObmcBlendFunc = void (*)(void* prediction, ptrdiff_t prediction_stride,
                               int width, int height,
                               const void* obmc_prediction,
                               ptrdiff_t obmc_prediction_stride);
using ObmcBlendFuncs = ObmcBlendFunc[kNumObmcDirections];

// Warp function signature. Section 7.11.3.5.
// This function applies warp filtering for each 8x8 block inside the current
// coding block. The filtering process is similar to 2d convolve filtering.
// The horizontal filter is applied followed by the vertical filter.
// The function has to calculate corresponding pixel positions before and
// after warping.
// |source| is the input reference frame buffer.
// |source_stride|, |source_width|, |source_height| are corresponding frame
// stride, width, and height. |source_stride| is given in bytes.
// |warp_params| is the matrix of warp motion: warp_params[i] = mN.
//         [x'     (m2 m3 m0   [x
//     z .  y'  =   m4 m5 m1 *  y
//          1]      m6 m7 1)    1]
// |subsampling_x/y| is the current frame's plane subsampling factor.
// |inter_round_bits_vertical| is the rounding precision used after vertical
// filtering (7 or 11). kInterRoundBitsHorizontal &
// kInterRoundBitsHorizontal12bpp can be used for the horizontal pass.
// |block_start_x| and |block_start_y| are the starting position the current
// coding block.
// |block_width| and |block_height| are width and height of the current coding
// block. |block_width| and |block_height| are at least 8.
// |alpha|, |beta|, |gamma|, |delta| are valid warp parameters. See the
// comments in the definition of struct GlobalMotion for the range of their
// values.
// |dest| is the output buffer. It is a predictor, whose type is int16_t.
// |dest_stride| is the stride, in units of int16_t.
//
// NOTE: WarpFunc assumes the source frame has left and right borders that
// extend the frame boundary pixels. The left and right borders must be at
// least 13 pixels wide. In addition, Warp_NEON() may read up to 14 bytes after
// a row in the |source| buffer. Therefore, there must be at least one extra
// padding byte after the right border of the last row in the source buffer.
using WarpFunc = void (*)(const void* source, ptrdiff_t source_stride,
                          int source_width, int source_height,
                          const int* warp_params, int subsampling_x,
                          int subsampling_y, int inter_round_bits_vertical,
                          int block_start_x, int block_start_y, int block_width,
                          int block_height, int16_t alpha, int16_t beta,
                          int16_t gamma, int16_t delta, uint16_t* dest,
                          ptrdiff_t dest_stride);

// Film grain synthesis function signature. Section 7.18.3.
// This function generates film grain noise and blends the noise with the
// decoded frame.
// |source_plane_y|, |source_plane_u|, and |source_plane_v| are the plane
// buffers of the decoded frame. They are blended with the film grain noise and
// written to |dest_plane_y|, |dest_plane_u|, and |dest_plane_v| as final
// output for display. |source_plane_p| and |dest_plane_p| (where p is y, u, or
// v) may point to the same buffer, in which case the film grain noise is added
// in place.
// |film_grain_params| are parameters read from frame header.
// |is_monochrome| is true indicates only Y plane needs to be processed.
// |color_matrix_is_identity| is true if the matrix_coefficients field in the
// sequence header's color config is is MC_IDENTITY.
// |width| is the upscaled width of the frame.
// |height| is the frame height.
// |subsampling_x| and |subsampling_y| are subsamplings for UV planes, not used
// if |is_monochrome| is true.
// Returns true on success, or false on failure (e.g., out of memory).
using FilmGrainSynthesisFunc = bool (*)(
    const void* source_plane_y, ptrdiff_t source_stride_y,
    const void* source_plane_u, ptrdiff_t source_stride_u,
    const void* source_plane_v, ptrdiff_t source_stride_v,
    const FilmGrainParams& film_grain_params, bool is_monochrome,
    bool color_matrix_is_identity, int width, int height, int subsampling_x,
    int subsampling_y, void* dest_plane_y, ptrdiff_t dest_stride_y,
    void* dest_plane_u, ptrdiff_t dest_stride_u, void* dest_plane_v,
    ptrdiff_t dest_stride_v);
//------------------------------------------------------------------------------

struct Dsp {
  IntraPredictorFuncs intra_predictors;
  DirectionalIntraPredictorZone1Func directional_intra_predictor_zone1;
  DirectionalIntraPredictorZone2Func directional_intra_predictor_zone2;
  DirectionalIntraPredictorZone3Func directional_intra_predictor_zone3;
  FilterIntraPredictorFunc filter_intra_predictor;
  CflIntraPredictorFuncs cfl_intra_predictors;
  CflSubsamplerFuncs cfl_subsamplers;
  IntraEdgeFilterFunc intra_edge_filter;
  IntraEdgeUpsamplerFunc intra_edge_upsampler;
  InverseTransformAddFuncs inverse_transforms;
  LoopFilterFuncs loop_filters;
  CdefDirectionFunc cdef_direction;
  CdefFilteringFunc cdef_filter;
  LoopRestorationFuncs loop_restorations;
  ConvolveFuncs convolve;
  ConvolveScaleFuncs convolve_scale;
  WeightMaskFuncs weight_mask;
  AverageBlendFunc average_blend;
  DistanceWeightedBlendFunc distance_weighted_blend;
  MaskBlendFuncs mask_blend;
  InterIntraMaskBlendFuncs8bpp inter_intra_mask_blend_8bpp;
  ObmcBlendFuncs obmc_blend;
  WarpFunc warp;
  FilmGrainSynthesisFunc film_grain_synthesis;
};

// Initializes function pointers based on build config and runtime environment.
// Must be called once before first use. This function is thread-safe.
void DspInit();

// Returns the appropriate Dsp table for |bitdepth| or nullptr if one doesn't
// exist.
const Dsp* GetDspTable(int bitdepth);

}  // namespace dsp

namespace dsp_internal {

// Returns true if a more highly optimized version of |func| is not defined for
// the associated bitdepth or if it is forcibly enabled with
// LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS. The define checked for |func| corresponds
// to the LIBGAV1_Dsp<bitdepth>bpp_|func| define in the header file associated
// with the module.
// |func| is one of:
//   - FunctionName, e.g., SelfGuidedFilter.
//   - [sub-table-index1][...-indexN] e.g.,
//     TransformSize4x4_IntraPredictorDc. The indices correspond to enum values
//     used as lookups with leading 'k' removed.
//
//  NEON support is the only extension available for ARM and it is always
//  required. Because of this restriction DSP_ENABLED_8BPP_NEON(func) is always
//  true and can be omitted.
#define DSP_ENABLED_8BPP_SSE4_1(func)  \
  (LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS || \
   LIBGAV1_Dsp8bpp_##func == LIBGAV1_CPU_SSE4_1)
#define DSP_ENABLED_10BPP_SSE4_1(func) \
  (LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS || \
   LIBGAV1_Dsp10bpp_##func == LIBGAV1_CPU_SSE4_1)

// Returns the appropriate Dsp table for |bitdepth| or nullptr if one doesn't
// exist. This version is meant for use by test or dsp/*Init() functions only.
dsp::Dsp* GetWritableDspTable(int bitdepth);

}  // namespace dsp_internal
}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_DSP_H_
