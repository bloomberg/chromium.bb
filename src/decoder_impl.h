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

#ifndef LIBGAV1_SRC_DECODER_IMPL_H_
#define LIBGAV1_SRC_DECODER_IMPL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "src/buffer_pool.h"
#include "src/dsp/constants.h"
#include "src/gav1/decoder_buffer.h"
#include "src/gav1/decoder_settings.h"
#include "src/gav1/status_code.h"
#include "src/loop_filter_mask.h"
#include "src/obu_parser.h"
#include "src/residual_buffer_pool.h"
#include "src/symbol_decoder_context.h"
#include "src/threading_strategy.h"
#include "src/tile.h"
#include "src/utils/array_2d.h"
#include "src/utils/block_parameters_holder.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"
#include "src/utils/queue.h"
#include "src/utils/segmentation_map.h"
#include "src/utils/types.h"

namespace libgav1 {

struct EncodedFrame : public Allocable {
  // The default constructor is invoked by the Queue<EncodedFrame>::Init()
  // method. Queue<> does not use the default-constructed elements, so it is
  // safe for the default constructor to not initialize the members.
  EncodedFrame() = default;
  EncodedFrame(const uint8_t* data, size_t size, int64_t user_private_data)
      : data(data), size(size), user_private_data(user_private_data) {}

  const uint8_t* data;
  size_t size;
  int64_t user_private_data;
};

struct DecoderState {
  ObuSequenceHeader sequence_header = {};
  // If true, sequence_header is valid.
  bool has_sequence_header = false;
  // reference_valid and reference_frame_id are used only if
  // sequence_header_.frame_id_numbers_present is true.
  // The reference_valid array is indexed by a reference picture slot number.
  // A value (boolean) in the array signifies whether the corresponding
  // reference picture slot is valid for use as a reference picture.
  std::array<bool, kNumReferenceFrameTypes> reference_valid = {};
  std::array<uint16_t, kNumReferenceFrameTypes> reference_frame_id = {};
  // A valid value of current_frame_id is an unsigned integer of at most 16
  // bits. -1 indicates current_frame_id is not initialized.
  int current_frame_id = -1;
  // The RefOrderHint array variable in the spec.
  std::array<uint8_t, kNumReferenceFrameTypes> reference_order_hint = {};
  // The OrderHint variable in the spec. Its value comes from either the
  // order_hint syntax element in the uncompressed header (if
  // show_existing_frame is false) or RefOrderHint[ frame_to_show_map_idx ]
  // (if show_existing_frame is true and frame_type is KEY_FRAME). See Section
  // 5.9.2 and Section 7.4.
  //
  // NOTE: When show_existing_frame is false, it is often more convenient to
  // just use the order_hint field of the frame header as OrderHint. So this
  // field is mainly used to update the state_.reference_order_hint array in
  // DecoderImpl::UpdateReferenceFrames().
  uint8_t order_hint = 0;
  // reference_frame_sign_bias[i] (a boolean) specifies the intended direction
  // of the motion vector in time for each reference frame.
  // * |false| indicates that the reference frame is a forwards reference (i.e.
  //   the reference frame is expected to be output before the current frame);
  // * |true| indicates that the reference frame is a backwards reference.
  // Note: reference_frame_sign_bias[0] (for kReferenceFrameIntra) is not used.
  std::array<bool, kNumReferenceFrameTypes> reference_frame_sign_bias = {};
  std::array<RefCountedBufferPtr, kNumReferenceFrameTypes> reference_frame;
  RefCountedBufferPtr current_frame;
  WedgeMaskArray wedge_masks;
  Array2D<TemporalMotionVector> motion_field_mv;
};

class DecoderImpl : public Allocable {
 public:
  // The constructor saves a const reference to |*settings|. Therefore
  // |*settings| must outlive the DecoderImpl object. On success, |*output|
  // contains a pointer to the newly-created DecoderImpl object. On failure,
  // |*output| is not modified.
  static StatusCode Create(const DecoderSettings* settings,
                           std::unique_ptr<DecoderImpl>* output);
  ~DecoderImpl();
  StatusCode EnqueueFrame(const uint8_t* data, size_t size,
                          int64_t user_private_data);
  StatusCode DequeueFrame(const DecoderBuffer** out_ptr);
  int GetMaxAllowedFrames() const {
    return settings_.frame_parallel ? settings_.threads : 1;
  }
  static constexpr int GetMaxBitdepth() {
    static_assert(LIBGAV1_MAX_BITDEPTH == 8 || LIBGAV1_MAX_BITDEPTH == 10,
                  "LIBGAV1_MAX_BITDEPTH must be 8 or 10.");
    return LIBGAV1_MAX_BITDEPTH;
  }

 private:
  explicit DecoderImpl(const DecoderSettings* settings);
  StatusCode Init();
  bool AllocateCurrentFrame(const ObuFrameHeader& frame_header);
  void ReleaseOutputFrame();
  // Populates buffer_ with values from |frame|. Adds a reference to |frame|
  // in output_frame_.
  StatusCode CopyFrameToOutputBuffer(const RefCountedBufferPtr& frame);
  StatusCode DecodeTiles(const ObuParser* obu);
  // Sets the current frame's segmentation map for two cases. The third case
  // is handled in Tile::DecodeBlock().
  void SetCurrentFrameSegmentationMap(const ObuFrameHeader& frame_header,
                                      const SegmentationMap* prev_segment_ids);
  // Section 7.20. Updates frames in the state_.reference_frame array with
  // state_.current_frame, based on the refresh_frame_flags bitmask.
  void UpdateReferenceFrames(int refresh_frame_flags);

  Queue<EncodedFrame> encoded_frames_;
  DecoderState state_;
  ThreadingStrategy threading_strategy_;
  SymbolDecoderContext symbol_decoder_context_;

  // TODO(vigneshv): Only support one buffer for now. Eventually this has to be
  // a vector or an array.
  DecoderBuffer buffer_ = {};
  // output_frame_ holds a reference to the output frame on behalf of buffer_.
  RefCountedBufferPtr output_frame_;

  BufferPool buffer_pool_;
  std::unique_ptr<ResidualBufferPool> residual_buffer_pool_;
  AlignedUniquePtr<uint8_t> threaded_window_buffer_;
  size_t threaded_window_buffer_size_ = 0;
  Array2D<TransformSize> inter_transform_sizes_;
  Array2D<int16_t> cdef_index_;
  DecoderScratchBufferPool decoder_scratch_buffer_pool_;

  LoopFilterMask loop_filter_mask_;

  const DecoderSettings& settings_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_DECODER_IMPL_H_
