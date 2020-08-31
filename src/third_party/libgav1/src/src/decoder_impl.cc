// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/decoder_impl.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iterator>
#include <new>
#include <utility>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/film_grain.h"
#include "src/frame_buffer_utils.h"
#include "src/frame_scratch_buffer.h"
#include "src/loop_filter_mask.h"
#include "src/loop_restoration_info.h"
#include "src/obu_parser.h"
#include "src/post_filter.h"
#include "src/prediction_mask.h"
#include "src/quantizer.h"
#include "src/threading_strategy.h"
#include "src/utils/blocking_counter.h"
#include "src/utils/common.h"
#include "src/utils/logging.h"
#include "src/utils/parameter_tree.h"
#include "src/utils/raw_bit_reader.h"
#include "src/utils/segmentation.h"
#include "src/utils/threadpool.h"
#include "src/yuv_buffer.h"

namespace libgav1 {
namespace {

constexpr int kMaxBlockWidth4x4 = 32;
constexpr int kMaxBlockHeight4x4 = 32;

// Computes the bottom border size in pixels. If CDEF, loop restoration or
// SuperRes is enabled, adds extra border pixels to facilitate those steps to
// happen nearly in-place (a few extra rows instead of an entire frame buffer).
int GetBottomBorderPixels(const bool do_cdef, const bool do_restoration,
                          const bool do_superres) {
  int border = kBorderPixels;
  if (do_cdef) border += 2 * kCdefBorder;
  if (do_restoration) border += 2 * kRestorationBorder;
  if (do_superres) border += 2 * kSuperResVerticalBorder;
  return border;
}

}  // namespace

// static
StatusCode DecoderImpl::Create(const DecoderSettings* settings,
                               std::unique_ptr<DecoderImpl>* output) {
  if (settings->threads <= 0) {
    LIBGAV1_DLOG(ERROR, "Invalid settings->threads: %d.", settings->threads);
    return kStatusInvalidArgument;
  }
  if (settings->frame_parallel) {
    if (settings->release_input_buffer == nullptr) {
      LIBGAV1_DLOG(ERROR,
                   "release_input_buffer callback must not be null when "
                   "frame_parallel is true.");
      return kStatusInvalidArgument;
    }
  }
  std::unique_ptr<DecoderImpl> impl(new (std::nothrow) DecoderImpl(settings));
  if (impl == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate DecoderImpl.");
    return kStatusOutOfMemory;
  }
  const StatusCode status = impl->Init();
  if (status != kStatusOk) return status;
  *output = std::move(impl);
  return kStatusOk;
}

DecoderImpl::DecoderImpl(const DecoderSettings* settings)
    : buffer_pool_(settings->on_frame_buffer_size_changed,
                   settings->get_frame_buffer, settings->release_frame_buffer,
                   settings->callback_private_data),
      settings_(*settings) {
  dsp::DspInit();
}

DecoderImpl::~DecoderImpl() {
  // The frame buffer references need to be released before |buffer_pool_| is
  // destroyed.
  ReleaseOutputFrame();
  for (auto& reference_frame : state_.reference_frame) {
    reference_frame = nullptr;
  }
}

StatusCode DecoderImpl::Init() {
  if (settings_.frame_parallel) {
#if defined(ENABLE_FRAME_PARALLEL)
    if (settings_.threads > 1) {
      if (!InitializeThreadPoolsForFrameParallel(settings_.threads,
                                                 &frame_thread_pool_)) {
        return kStatusOutOfMemory;
      }
      // TODO(b/142583029): Frame parallel decoding with in-frame
      // multi-threading is not yet implemented. Until then, we force
      // settings_.threads to 1 when frame parallel decoding is enabled.
      settings_.threads = 1;
    }
#else
    LIBGAV1_DLOG(
        ERROR, "Frame parallel decoding is not implemented, ignoring setting.");
#endif  // defined(ENABLE_FRAME_PARALLEL)
  }
  const int max_allowed_frames = GetMaxAllowedFrames();
  assert(max_allowed_frames > 0);
  if (!temporal_units_.Init(max_allowed_frames)) {
    LIBGAV1_DLOG(ERROR, "temporal_units_.Init() failed.");
    return kStatusOutOfMemory;
  }
  if (!GenerateWedgeMask(&wedge_masks_)) {
    LIBGAV1_DLOG(ERROR, "GenerateWedgeMask() failed.");
    return kStatusOutOfMemory;
  }
  return kStatusOk;
}

StatusCode DecoderImpl::EnqueueFrame(const uint8_t* data, size_t size,
                                     int64_t user_private_data,
                                     void* buffer_private_data) {
  if (data == nullptr || size == 0) return kStatusInvalidArgument;
  if (abort_) return kStatusUnknownError;
  if (temporal_units_.Full()) {
    return kStatusTryAgain;
  }
  TemporalUnit temporal_unit(data, size, user_private_data,
                             buffer_private_data);
  temporal_units_.Push(std::move(temporal_unit));
  return IsFrameParallel() ? SignalFailure(ParseAndSchedule()) : kStatusOk;
}

StatusCode DecoderImpl::SignalFailure(StatusCode status) {
  if (status == kStatusOk || status == kStatusTryAgain) return status;
  abort_ = true;
  failure_status_ = status;
  // Make sure all waiting threads exit.
  buffer_pool_.Abort();
  frame_thread_pool_ = nullptr;
  while (!temporal_units_.Empty()) {
    if (settings_.release_input_buffer != nullptr) {
      settings_.release_input_buffer(
          settings_.callback_private_data,
          temporal_units_.Front().buffer_private_data);
    }
    temporal_units_.Pop();
  }
  return status;
}

// DequeueFrame() follows the following policy to avoid holding unnecessary
// frame buffer references in output_frame_: output_frame_ must be null when
// DequeueFrame() returns false.
StatusCode DecoderImpl::DequeueFrame(const DecoderBuffer** out_ptr) {
  if (out_ptr == nullptr) {
    LIBGAV1_DLOG(ERROR, "Invalid argument: out_ptr == nullptr.");
    return kStatusInvalidArgument;
  }
  // We assume a call to DequeueFrame() indicates that the caller is no longer
  // using the previous output frame, so we can release it.
  ReleaseOutputFrame();
  if (temporal_units_.Empty()) {
    // No input frames to decode. Not an error.
    *out_ptr = nullptr;
    return kStatusOk;
  }
  const TemporalUnit& temporal_unit = temporal_units_.Front();
  if (!IsFrameParallel()) {
    // DequeueFrame() is a blocking call in this case. Just decode the next
    // available temporal unit and return.
    const StatusCode status = DecodeTemporalUnit(temporal_unit, out_ptr);
    if (settings_.release_input_buffer != nullptr) {
      settings_.release_input_buffer(settings_.callback_private_data,
                                     temporal_unit.buffer_private_data);
    }
    temporal_units_.Pop();
    return status;
  }
  if (settings_.blocking_dequeue) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!temporal_unit.decoded && !abort_) {
      decoded_condvar_.wait(lock);
    }
  } else {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!temporal_unit.decoded && !abort_) return kStatusTryAgain;
  }
  if (abort_) {
    return SignalFailure(failure_status_);
  }
  if (settings_.release_input_buffer != nullptr) {
    settings_.release_input_buffer(settings_.callback_private_data,
                                   temporal_unit.buffer_private_data);
  }
  if (temporal_unit.status != kStatusOk) {
    temporal_units_.Pop();
    return SignalFailure(temporal_unit.status);
  }
  if (!temporal_unit.has_displayable_frame) {
    *out_ptr = nullptr;
    temporal_units_.Pop();
    return kStatusOk;
  }
  StatusCode status = CopyFrameToOutputBuffer(temporal_unit.output_frame);
  if (status != kStatusOk) {
    temporal_units_.Pop();
    return SignalFailure(status);
  }
  buffer_.user_private_data = temporal_unit.user_private_data;
  *out_ptr = &buffer_;
  temporal_units_.Pop();
  return kStatusOk;
}

StatusCode DecoderImpl::ParseAndSchedule() {
  TemporalUnit& temporal_unit = temporal_units_.Back();
  std::unique_ptr<ObuParser> obu(new (std::nothrow) ObuParser(
      temporal_unit.data, temporal_unit.size, &buffer_pool_, &state_));
  if (obu == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate OBU parser.");
    return kStatusOutOfMemory;
  }
  if (has_sequence_header_) {
    obu->set_sequence_header(sequence_header_);
  }
  StatusCode status;
  int position_in_temporal_unit = 0;
  while (obu->HasData()) {
    RefCountedBufferPtr current_frame;
    status = obu->ParseOneFrame(&current_frame);
    if (status != kStatusOk) {
      LIBGAV1_DLOG(ERROR, "Failed to parse OBU.");
      return status;
    }
    if (IsNewSequenceHeader(*obu)) {
      const ObuSequenceHeader& sequence_header = obu->sequence_header();
      const Libgav1ImageFormat image_format =
          ComposeImageFormat(sequence_header.color_config.is_monochrome,
                             sequence_header.color_config.subsampling_x,
                             sequence_header.color_config.subsampling_y);
      const int max_bottom_border =
          GetBottomBorderPixels(/*do_cdef=*/true, /*do_restoration=*/true,
                                /*do_superres=*/true);
      // TODO(vigneshv): This may not be the right place to call this callback
      // for the frame parallel case. Investigate and fix it.
      if (!buffer_pool_.OnFrameBufferSizeChanged(
              sequence_header.color_config.bitdepth, image_format,
              sequence_header.max_frame_width, sequence_header.max_frame_height,
              kBorderPixels, kBorderPixels, kBorderPixels, max_bottom_border)) {
        LIBGAV1_DLOG(ERROR, "buffer_pool_.OnFrameBufferSizeChanged failed.");
        return kStatusUnknownError;
      }
    }
    // This can happen when there are multiple spatial/temporal layers and if
    // all the layers are outside the current operating point.
    if (current_frame == nullptr) {
      continue;
    }
    if (!temporal_unit.frames.emplace_back(obu.get(), state_, &temporal_unit,
                                           current_frame,
                                           position_in_temporal_unit++)) {
      LIBGAV1_DLOG(ERROR, "temporal_unit.frames.emplace_back failed.");
      return kStatusOutOfMemory;
    }
    state_.UpdateReferenceFrames(current_frame,
                                 obu->frame_header().refresh_frame_flags);
  }
  if (temporal_unit.frames.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    temporal_unit.has_displayable_frame = false;
    temporal_unit.decoded = true;
    decoded_condvar_.notify_one();
    return kStatusOk;
  }
  for (auto& frame : temporal_unit.frames) {
    EncodedFrame* const encoded_frame = &frame;
    frame_thread_pool_->Schedule([this, encoded_frame]() {
      if (abort_) return;
      const StatusCode status = DecodeFrame(encoded_frame);
      if (abort_) return;
      encoded_frame->state = {};
      encoded_frame->frame = nullptr;
      TemporalUnit& temporal_unit = encoded_frame->temporal_unit;
      std::lock_guard<std::mutex> lock(mutex_);
      // temporal_unit's status defaults to kStatusOk. So we need to set it only
      // on error. If |abort_| is true at this point, it means that there has
      // already been a failure. So we don't care about this subsequent failure.
      // We will simply return the error code of the first failure.
      if (status != kStatusOk) {
        temporal_unit.status = status;
        if (!abort_) {
          abort_ = true;
          failure_status_ = status;
        }
      }
      temporal_unit.decoded =
          ++temporal_unit.decoded_count == temporal_unit.frames.size();
      if (temporal_unit.decoded || abort_) {
        decoded_condvar_.notify_one();
      }
    });
  }
  return kStatusOk;
}

StatusCode DecoderImpl::DecodeFrame(EncodedFrame* const encoded_frame) {
  const ObuSequenceHeader& sequence_header = encoded_frame->sequence_header;
  const ObuFrameHeader& frame_header = encoded_frame->frame_header;
  const Vector<ObuTileGroup>& tile_groups = encoded_frame->tile_groups;
  RefCountedBufferPtr current_frame = std::move(encoded_frame->frame);

  StatusCode status;
  if (!frame_header.show_existing_frame) {
    if (tile_groups.empty()) {
      // This means that the last call to ParseOneFrame() did not actually
      // have any tile groups. This could happen in rare cases (for example,
      // if there is a Metadata OBU after the TileGroup OBU). We currently do
      // not have a reason to handle those cases, so we simply continue.
      return kStatusOk;
    }
    std::unique_ptr<FrameScratchBuffer> frame_scratch_buffer =
        frame_scratch_buffer_pool_.Get();
    if (frame_scratch_buffer == nullptr) {
      LIBGAV1_DLOG(ERROR, "Error when getting FrameScratchBuffer.");
      return kStatusOutOfMemory;
    }
    status = DecodeTiles(sequence_header, frame_header, tile_groups,
                         encoded_frame->state, frame_scratch_buffer.get(),
                         current_frame.get());
    frame_scratch_buffer_pool_.Release(std::move(frame_scratch_buffer));
    if (status != kStatusOk) {
      return status;
    }
  } else {
    if (!current_frame->WaitUntilDecoded()) {
      return kStatusUnknownError;
    }
  }
  if (!frame_header.show_frame && !frame_header.show_existing_frame) {
    // This frame is not displayable. Not an error.
    return kStatusOk;
  }
  RefCountedBufferPtr film_grain_frame;
  status = ApplyFilmGrain(sequence_header, frame_header, current_frame,
                          &film_grain_frame, /*thread_pool=*/nullptr);
  if (status != kStatusOk) {
    return status;
  }
  current_frame = std::move(film_grain_frame);
  TemporalUnit& temporal_unit = encoded_frame->temporal_unit;
  std::lock_guard<std::mutex> lock(mutex_);
  if (temporal_unit.has_displayable_frame &&
      temporal_unit.output_frame_position >
          encoded_frame->position_in_temporal_unit) {
    // A displayable frame was already found and its position in the temporal
    // unit is greater than the current frame's position.  This can happen if
    // there are multiple spatial/temporal layers. We don't care about it for
    // now, so simply return the last displayable frame.
    // TODO(b/129153372): Add support for outputting multiple spatial/temporal
    // layers.
    return kStatusOk;
  }
  temporal_unit.has_displayable_frame = true;
  temporal_unit.output_frame = std::move(current_frame);
  temporal_unit.output_frame_position =
      encoded_frame->position_in_temporal_unit;
  return kStatusOk;
}

StatusCode DecoderImpl::DecodeTemporalUnit(const TemporalUnit& temporal_unit,
                                           const DecoderBuffer** out_ptr) {
  std::unique_ptr<ObuParser> obu(new (std::nothrow) ObuParser(
      temporal_unit.data, temporal_unit.size, &buffer_pool_, &state_));
  if (obu == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate OBU parser.");
    return kStatusOutOfMemory;
  }
  if (has_sequence_header_) {
    obu->set_sequence_header(sequence_header_);
  }
  RefCountedBufferPtr current_frame;
  RefCountedBufferPtr displayable_frame;
  StatusCode status;
  while (obu->HasData()) {
    status = obu->ParseOneFrame(&current_frame);
    if (status != kStatusOk) {
      LIBGAV1_DLOG(ERROR, "Failed to parse OBU.");
      return status;
    }
    if (IsNewSequenceHeader(*obu)) {
      const ObuSequenceHeader& sequence_header = obu->sequence_header();
      const Libgav1ImageFormat image_format =
          ComposeImageFormat(sequence_header.color_config.is_monochrome,
                             sequence_header.color_config.subsampling_x,
                             sequence_header.color_config.subsampling_y);
      const int max_bottom_border =
          GetBottomBorderPixels(/*do_cdef=*/true, /*do_restoration=*/true,
                                /*do_superres=*/true);
      if (!buffer_pool_.OnFrameBufferSizeChanged(
              sequence_header.color_config.bitdepth, image_format,
              sequence_header.max_frame_width, sequence_header.max_frame_height,
              kBorderPixels, kBorderPixels, kBorderPixels, max_bottom_border)) {
        LIBGAV1_DLOG(ERROR, "buffer_pool_.OnFrameBufferSizeChanged failed.");
        return kStatusUnknownError;
      }
    }
    if (!obu->frame_header().show_existing_frame) {
      if (obu->tile_groups().empty()) {
        // This means that the last call to ParseOneFrame() did not actually
        // have any tile groups. This could happen in rare cases (for example,
        // if there is a Metadata OBU after the TileGroup OBU). We currently do
        // not have a reason to handle those cases, so we simply continue.
        continue;
      }
      std::unique_ptr<FrameScratchBuffer> frame_scratch_buffer =
          frame_scratch_buffer_pool_.Get();
      if (frame_scratch_buffer == nullptr) {
        LIBGAV1_DLOG(ERROR, "Error when getting FrameScratchBuffer.");
        return kStatusOutOfMemory;
      }
      status = DecodeTiles(obu->sequence_header(), obu->frame_header(),
                           obu->tile_groups(), state_,
                           frame_scratch_buffer.get(), current_frame.get());
      frame_scratch_buffer_pool_.Release(std::move(frame_scratch_buffer));
      if (status != kStatusOk) {
        return status;
      }
    }
    state_.UpdateReferenceFrames(current_frame,
                                 obu->frame_header().refresh_frame_flags);
    if (obu->frame_header().show_frame ||
        obu->frame_header().show_existing_frame) {
      if (displayable_frame != nullptr) {
        // This can happen if there are multiple spatial/temporal layers. We
        // don't care about it for now, so simply return the last displayable
        // frame.
        // TODO(b/129153372): Add support for outputting multiple
        // spatial/temporal layers.
        LIBGAV1_DLOG(
            WARNING,
            "More than one displayable frame found. Using the last one.");
      }
      displayable_frame = std::move(current_frame);
      RefCountedBufferPtr film_grain_frame;
      std::unique_ptr<FrameScratchBuffer> frame_scratch_buffer =
          frame_scratch_buffer_pool_.Get();
      if (frame_scratch_buffer == nullptr) {
        LIBGAV1_DLOG(ERROR, "Error when getting FrameScratchBuffer.");
        return kStatusOutOfMemory;
      }
      status = ApplyFilmGrain(
          obu->sequence_header(), obu->frame_header(), displayable_frame,
          &film_grain_frame,
          frame_scratch_buffer->threading_strategy.film_grain_thread_pool());
      frame_scratch_buffer_pool_.Release(std::move(frame_scratch_buffer));
      if (status != kStatusOk) return status;
      displayable_frame = std::move(film_grain_frame);
    }
  }
  if (displayable_frame == nullptr) {
    // No displayable frame in the temporal unit. Not an error.
    *out_ptr = nullptr;
    return kStatusOk;
  }
  status = CopyFrameToOutputBuffer(displayable_frame);
  if (status != kStatusOk) {
    return status;
  }
  buffer_.user_private_data = temporal_unit.user_private_data;
  *out_ptr = &buffer_;
  return kStatusOk;
}

bool DecoderImpl::AllocateCurrentFrame(RefCountedBuffer* const current_frame,
                                       const ColorConfig& color_config,
                                       const ObuFrameHeader& frame_header,
                                       int left_border, int right_border,
                                       int top_border, int bottom_border) {
  current_frame->set_chroma_sample_position(
      color_config.chroma_sample_position);
  return current_frame->Realloc(
      color_config.bitdepth, color_config.is_monochrome,
      frame_header.upscaled_width, frame_header.height,
      color_config.subsampling_x, color_config.subsampling_y, left_border,
      right_border, top_border, bottom_border);
}

StatusCode DecoderImpl::CopyFrameToOutputBuffer(
    const RefCountedBufferPtr& frame) {
  YuvBuffer* yuv_buffer = frame->buffer();

  buffer_.chroma_sample_position = frame->chroma_sample_position();

  if (yuv_buffer->is_monochrome()) {
    buffer_.image_format = kImageFormatMonochrome400;
  } else {
    if (yuv_buffer->subsampling_x() == 0 && yuv_buffer->subsampling_y() == 0) {
      buffer_.image_format = kImageFormatYuv444;
    } else if (yuv_buffer->subsampling_x() == 1 &&
               yuv_buffer->subsampling_y() == 0) {
      buffer_.image_format = kImageFormatYuv422;
    } else if (yuv_buffer->subsampling_x() == 1 &&
               yuv_buffer->subsampling_y() == 1) {
      buffer_.image_format = kImageFormatYuv420;
    } else {
      LIBGAV1_DLOG(ERROR,
                   "Invalid chroma subsampling values: cannot determine buffer "
                   "image format.");
      return kStatusInvalidArgument;
    }
  }
  buffer_.color_range = sequence_header_.color_config.color_range;
  buffer_.color_primary = sequence_header_.color_config.color_primary;
  buffer_.transfer_characteristics =
      sequence_header_.color_config.transfer_characteristics;
  buffer_.matrix_coefficients =
      sequence_header_.color_config.matrix_coefficients;

  buffer_.bitdepth = yuv_buffer->bitdepth();
  const int num_planes =
      yuv_buffer->is_monochrome() ? kMaxPlanesMonochrome : kMaxPlanes;
  int plane = 0;
  for (; plane < num_planes; ++plane) {
    buffer_.stride[plane] = yuv_buffer->stride(plane);
    buffer_.plane[plane] = yuv_buffer->data(plane);
    buffer_.displayed_width[plane] = yuv_buffer->width(plane);
    buffer_.displayed_height[plane] = yuv_buffer->height(plane);
  }
  for (; plane < kMaxPlanes; ++plane) {
    buffer_.stride[plane] = 0;
    buffer_.plane[plane] = nullptr;
    buffer_.displayed_width[plane] = 0;
    buffer_.displayed_height[plane] = 0;
  }
  buffer_.buffer_private_data = frame->buffer_private_data();
  output_frame_ = frame;
  return kStatusOk;
}

void DecoderImpl::ReleaseOutputFrame() {
  for (auto& plane : buffer_.plane) {
    plane = nullptr;
  }
  output_frame_ = nullptr;
}

StatusCode DecoderImpl::DecodeTiles(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header, const Vector<ObuTileGroup>& tile_groups,
    const DecoderState& state, FrameScratchBuffer* const frame_scratch_buffer,
    RefCountedBuffer* const current_frame) {
  frame_scratch_buffer->tile_scratch_buffer_pool.Reset(
      sequence_header.color_config.bitdepth);
  if (IsFrameParallel()) {
    // We can parse the current frame if all the reference frames have been
    // parsed.
    for (int i = 0; i < kNumReferenceFrameTypes; ++i) {
      if (!state.reference_valid[i] || state.reference_frame[i] == nullptr) {
        continue;
      }
      if (!state.reference_frame[i]->WaitUntilParsed()) {
        return kStatusUnknownError;
      }
    }
  }
  if (PostFilter::DoDeblock(frame_header, settings_.post_filter_mask)) {
    if (kDeblockFilterBitMask && !frame_scratch_buffer->loop_filter_mask.Reset(
                                     frame_header.width, frame_header.height)) {
      LIBGAV1_DLOG(ERROR, "Failed to allocate memory for loop filter masks.");
      return kStatusOutOfMemory;
    }
  }
  if (!frame_scratch_buffer->loop_restoration_info.Reset(
          &frame_header.loop_restoration, frame_header.upscaled_width,
          frame_header.height, sequence_header.color_config.subsampling_x,
          sequence_header.color_config.subsampling_y,
          sequence_header.color_config.is_monochrome)) {
    LIBGAV1_DLOG(ERROR,
                 "Failed to allocate memory for loop restoration info units.");
    return kStatusOutOfMemory;
  }
  const bool do_cdef =
      PostFilter::DoCdef(frame_header, settings_.post_filter_mask);
  const int num_planes = sequence_header.color_config.is_monochrome
                             ? kMaxPlanesMonochrome
                             : kMaxPlanes;
  const bool do_restoration = PostFilter::DoRestoration(
      frame_header.loop_restoration, settings_.post_filter_mask, num_planes);
  const bool do_superres =
      PostFilter::DoSuperRes(frame_header, settings_.post_filter_mask);
  // Use kBorderPixels for the left, right, and top borders. Only the bottom
  // border may need to be bigger. SuperRes border is needed only if we are
  // applying SuperRes in-place which is being done only in single threaded
  // mode.
  const int bottom_border = GetBottomBorderPixels(
      do_cdef, do_restoration,
      do_superres &&
          frame_scratch_buffer->threading_strategy.post_filter_thread_pool() ==
              nullptr);
  if (!AllocateCurrentFrame(current_frame, sequence_header.color_config,
                            frame_header, kBorderPixels, kBorderPixels,
                            kBorderPixels, bottom_border)) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate memory for the decoder buffer.");
    return kStatusOutOfMemory;
  }
  if (sequence_header.enable_cdef) {
    if (!frame_scratch_buffer->cdef_index.Reset(
            DivideBy16(frame_header.rows4x4 + kMaxBlockHeight4x4),
            DivideBy16(frame_header.columns4x4 + kMaxBlockWidth4x4),
            /*zero_initialize=*/false)) {
      LIBGAV1_DLOG(ERROR, "Failed to allocate memory for cdef index.");
      return kStatusOutOfMemory;
    }
  }
  if (!frame_scratch_buffer->inter_transform_sizes.Reset(
          frame_header.rows4x4 + kMaxBlockHeight4x4,
          frame_header.columns4x4 + kMaxBlockWidth4x4,
          /*zero_initialize=*/false)) {
    LIBGAV1_DLOG(ERROR, "Failed to allocate memory for inter_transform_sizes.");
    return kStatusOutOfMemory;
  }
  if (frame_header.use_ref_frame_mvs) {
    if (!frame_scratch_buffer->motion_field.mv.Reset(
            DivideBy2(frame_header.rows4x4), DivideBy2(frame_header.columns4x4),
            /*zero_initialize=*/false) ||
        !frame_scratch_buffer->motion_field.reference_offset.Reset(
            DivideBy2(frame_header.rows4x4), DivideBy2(frame_header.columns4x4),
            /*zero_initialize=*/false)) {
      LIBGAV1_DLOG(ERROR,
                   "Failed to allocate memory for temporal motion vectors.");
      return kStatusOutOfMemory;
    }

    // For each motion vector, only mv[0] needs to be initialized to
    // kInvalidMvValue, mv[1] is not necessary to be initialized and can be
    // set to an arbitrary value. For simplicity, mv[1] is set to 0.
    // The following memory initialization of contiguous memory is very fast. It
    // is not recommended to make the initialization multi-threaded, unless the
    // memory which needs to be initialized in each thread is still contiguous.
    MotionVector invalid_mv;
    invalid_mv.mv[0] = kInvalidMvValue;
    invalid_mv.mv[1] = 0;
    MotionVector* const motion_field_mv =
        &frame_scratch_buffer->motion_field.mv[0][0];
    std::fill(motion_field_mv,
              motion_field_mv + frame_scratch_buffer->motion_field.mv.size(),
              invalid_mv);
  }

  // The addition of kMaxBlockHeight4x4 and kMaxBlockWidth4x4 is necessary so
  // that the block parameters cache can be filled in for the last row/column
  // without having to check for boundary conditions.
  BlockParametersHolder block_parameters_holder(
      frame_header.rows4x4 + kMaxBlockHeight4x4,
      frame_header.columns4x4 + kMaxBlockWidth4x4,
      sequence_header.use_128x128_superblock);
  if (!block_parameters_holder.Init()) {
    return kStatusOutOfMemory;
  }
  const dsp::Dsp* const dsp =
      dsp::GetDspTable(sequence_header.color_config.bitdepth);
  if (dsp == nullptr) {
    LIBGAV1_DLOG(ERROR, "Failed to get the dsp table for bitdepth %d.",
                 sequence_header.color_config.bitdepth);
    return kStatusInternalError;
  }
  // If prev_segment_ids is a null pointer, it is treated as if it pointed to
  // a segmentation map containing all 0s.
  const SegmentationMap* prev_segment_ids = nullptr;
  if (frame_header.primary_reference_frame == kPrimaryReferenceNone) {
    frame_scratch_buffer->symbol_decoder_context.Initialize(
        frame_header.quantizer.base_index);
  } else {
    const int index =
        frame_header
            .reference_frame_index[frame_header.primary_reference_frame];
    const RefCountedBuffer* prev_frame = state.reference_frame[index].get();
    frame_scratch_buffer->symbol_decoder_context = prev_frame->FrameContext();
    if (frame_header.segmentation.enabled &&
        prev_frame->columns4x4() == frame_header.columns4x4 &&
        prev_frame->rows4x4() == frame_header.rows4x4) {
      prev_segment_ids = prev_frame->segmentation_map();
    }
  }

  const uint8_t tile_size_bytes = frame_header.tile_info.tile_size_bytes;
  const int tile_count = tile_groups.back().end + 1;
  assert(tile_count >= 1);
  Vector<std::unique_ptr<Tile>> tiles;
  if (!tiles.reserve(tile_count)) {
    LIBGAV1_DLOG(ERROR, "tiles.reserve(%d) failed.\n", tile_count);
    return kStatusOutOfMemory;
  }
  ThreadingStrategy& threading_strategy =
      frame_scratch_buffer->threading_strategy;
  if (!threading_strategy.Reset(frame_header, settings_.threads)) {
    return kStatusOutOfMemory;
  }

  if (threading_strategy.row_thread_pool(0) != nullptr || IsFrameParallel()) {
    const int block_width4x4_minus_one =
        sequence_header.use_128x128_superblock ? 31 : 15;
    const int block_width4x4_log2 =
        sequence_header.use_128x128_superblock ? 5 : 4;
    const int superblock_rows =
        (frame_header.rows4x4 + block_width4x4_minus_one) >>
        block_width4x4_log2;
    const int superblock_columns =
        (frame_header.columns4x4 + block_width4x4_minus_one) >>
        block_width4x4_log2;
    if (!frame_scratch_buffer->superblock_state.Reset(superblock_rows,
                                                      superblock_columns)) {
      LIBGAV1_DLOG(ERROR, "Failed to allocate super_block_state.\n");
      return kStatusOutOfMemory;
    }
    if (frame_scratch_buffer->residual_buffer_pool == nullptr) {
      frame_scratch_buffer->residual_buffer_pool.reset(
          new (std::nothrow) ResidualBufferPool(
              sequence_header.use_128x128_superblock,
              sequence_header.color_config.subsampling_x,
              sequence_header.color_config.subsampling_y,
              sequence_header.color_config.bitdepth == 8 ? sizeof(int16_t)
                                                         : sizeof(int32_t)));
      if (frame_scratch_buffer->residual_buffer_pool == nullptr) {
        LIBGAV1_DLOG(ERROR, "Failed to allocate residual buffer.\n");
        return kStatusOutOfMemory;
      }
    } else {
      frame_scratch_buffer->residual_buffer_pool->Reset(
          sequence_header.use_128x128_superblock,
          sequence_header.color_config.subsampling_x,
          sequence_header.color_config.subsampling_y,
          sequence_header.color_config.bitdepth == 8 ? sizeof(int16_t)
                                                     : sizeof(int32_t));
    }
  }

  if (threading_strategy.post_filter_thread_pool() != nullptr &&
      (do_cdef || do_restoration)) {
    const int window_buffer_width = PostFilter::GetWindowBufferWidth(
        threading_strategy.post_filter_thread_pool(), frame_header);
    size_t threaded_window_buffer_size =
        window_buffer_width *
        PostFilter::GetWindowBufferHeight(
            threading_strategy.post_filter_thread_pool(), frame_header) *
        (sequence_header.color_config.bitdepth == 8 ? sizeof(uint8_t)
                                                    : sizeof(uint16_t));
    if (do_cdef) {
      // TODO(chengchen): for cdef U, V planes, if there's subsampling, we can
      // use smaller buffer.
      threaded_window_buffer_size *= num_planes;
    }
    // To avoid false sharing, PostFilter's window width in bytes should be a
    // multiple of the cache line size. For simplicity, we check the window
    // width in pixels.
    assert(window_buffer_width % kCacheLineSize == 0);
    if (!frame_scratch_buffer->threaded_window_buffer.Resize(
            threaded_window_buffer_size)) {
      LIBGAV1_DLOG(ERROR,
                   "Failed to resize threaded loop restoration buffer.\n");
      return kStatusOutOfMemory;
    }
  }

  if (do_cdef && do_restoration) {
    // We need to store 4 rows per 64x64 unit.
    const int num_deblock_units = MultiplyBy4(Ceil(frame_header.rows4x4, 16));
    // subsampling_y is set to zero irrespective of the actual frame's
    // subsampling since we need to store exactly |num_deblock_units| rows of
    // the deblocked pixels.
    if (!frame_scratch_buffer->deblock_buffer.Realloc(
            sequence_header.color_config.bitdepth,
            sequence_header.color_config.is_monochrome,
            frame_header.upscaled_width, num_deblock_units,
            sequence_header.color_config.subsampling_x,
            /*subsampling_y=*/0, kBorderPixels, kBorderPixels, kBorderPixels,
            kBorderPixels, nullptr, nullptr, nullptr)) {
      return kStatusOutOfMemory;
    }
  }

  if (do_superres) {
    const int num_threads =
        1 + ((threading_strategy.post_filter_thread_pool() == nullptr)
                 ? 0
                 : threading_strategy.post_filter_thread_pool()->num_threads());
    const size_t superres_line_buffer_size =
        num_threads *
        ((MultiplyBy4(frame_header.columns4x4) +
          MultiplyBy2(kSuperResHorizontalBorder)) *
         (sequence_header.color_config.bitdepth == 8 ? sizeof(uint8_t)
                                                     : sizeof(uint16_t)));
    if (!frame_scratch_buffer->superres_line_buffer.Resize(
            superres_line_buffer_size)) {
      LIBGAV1_DLOG(ERROR, "Failed to resize superres line buffer.\n");
      return kStatusOutOfMemory;
    }
  }

  PostFilter post_filter(
      frame_header, sequence_header, &frame_scratch_buffer->loop_filter_mask,
      frame_scratch_buffer->cdef_index,
      frame_scratch_buffer->inter_transform_sizes,
      &frame_scratch_buffer->loop_restoration_info, &block_parameters_holder,
      current_frame->buffer(), &frame_scratch_buffer->deblock_buffer, dsp,
      threading_strategy.post_filter_thread_pool(),
      frame_scratch_buffer->threaded_window_buffer.get(),
      frame_scratch_buffer->superres_line_buffer.get(),
      settings_.post_filter_mask);
  // The Tile class must make use of a separate buffer to store the unfiltered
  // pixels for the intra prediction of the next superblock row. This is done
  // only when one of the following conditions are true:
  //   * frame_parallel is true.
  //   * settings_.threads == 1.
  // In the non-frame-parallel multi-threaded case, we do not run the post
  // filters in the decode loop. So this buffer need not be used.
  const bool use_intra_prediction_buffer =
      IsFrameParallel() || settings_.threads == 1;
  SymbolDecoderContext saved_symbol_decoder_context;
  int tile_index = 0;
  BlockingCounterWithStatus pending_tiles(tile_count);
  for (const auto& tile_group : tile_groups) {
    size_t bytes_left = tile_group.data_size;
    size_t byte_offset = 0;
    // The for loop in 5.11.1.
    for (int tile_number = tile_group.start; tile_number <= tile_group.end;
         ++tile_number) {
      size_t tile_size = 0;
      if (tile_number != tile_group.end) {
        RawBitReader bit_reader(tile_group.data + byte_offset, bytes_left);
        if (!bit_reader.ReadLittleEndian(tile_size_bytes, &tile_size)) {
          LIBGAV1_DLOG(ERROR, "Could not read tile size for tile #%d",
                       tile_number);
          return kStatusBitstreamError;
        }
        ++tile_size;
        byte_offset += tile_size_bytes;
        bytes_left -= tile_size_bytes;
        if (tile_size > bytes_left) {
          LIBGAV1_DLOG(ERROR, "Invalid tile size %zu for tile #%d", tile_size,
                       tile_number);
          return kStatusBitstreamError;
        }
      } else {
        tile_size = bytes_left;
      }

      std::unique_ptr<Tile> tile = Tile::Create(
          tile_number, tile_group.data + byte_offset, tile_size,
          sequence_header, frame_header, current_frame, state,
          frame_scratch_buffer, wedge_masks_, &saved_symbol_decoder_context,
          prev_segment_ids, &post_filter, &block_parameters_holder, dsp,
          threading_strategy.row_thread_pool(tile_index++), &pending_tiles,
          IsFrameParallel(), use_intra_prediction_buffer);
      if (tile == nullptr) {
        LIBGAV1_DLOG(ERROR, "Failed to create tile.");
        return kStatusOutOfMemory;
      }
      tiles.push_back_unchecked(std::move(tile));

      byte_offset += tile_size;
      bytes_left -= tile_size;
    }
  }
  assert(tiles.size() == static_cast<size_t>(tile_count));
  if (IsFrameParallel()) {
    return DecodeTilesFrameParallel(
        sequence_header, frame_header, tiles, saved_symbol_decoder_context,
        prev_segment_ids, frame_scratch_buffer, &post_filter, current_frame);
  }
  StatusCode status;
  if (settings_.threads == 1) {
    status = DecodeTilesNonFrameParallel(sequence_header, frame_header, tiles,
                                         frame_scratch_buffer, &post_filter);
  } else {
    status = DecodeTilesThreadedNonFrameParallel(
        sequence_header, frame_header, tiles, tile_groups,
        block_parameters_holder, frame_scratch_buffer, &post_filter,
        &pending_tiles);
  }
  if (status != kStatusOk) return status;
  if (frame_header.enable_frame_end_update_cdf) {
    frame_scratch_buffer->symbol_decoder_context = saved_symbol_decoder_context;
  }
  current_frame->SetFrameContext(frame_scratch_buffer->symbol_decoder_context);
  SetCurrentFrameSegmentationMap(frame_header, prev_segment_ids, current_frame);
  return kStatusOk;
}

StatusCode DecoderImpl::DecodeTilesNonFrameParallel(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header,
    const Vector<std::unique_ptr<Tile>>& tiles,
    FrameScratchBuffer* const frame_scratch_buffer,
    PostFilter* const post_filter) {
  // Decode in superblock row order.
  const int block_width4x4 = sequence_header.use_128x128_superblock ? 32 : 16;
  std::unique_ptr<TileScratchBuffer> tile_scratch_buffer =
      frame_scratch_buffer->tile_scratch_buffer_pool.Get();
  if (tile_scratch_buffer == nullptr) return kLibgav1StatusOutOfMemory;
  for (int row4x4 = 0; row4x4 < frame_header.rows4x4;
       row4x4 += block_width4x4) {
    for (const auto& tile_ptr : tiles) {
      if (!tile_ptr->ProcessSuperBlockRow<kProcessingModeParseAndDecode, true>(
              row4x4, tile_scratch_buffer.get())) {
        return kLibgav1StatusUnknownError;
      }
    }
    post_filter->ApplyFilteringForOneSuperBlockRow(
        row4x4, block_width4x4,
        row4x4 + block_width4x4 >= frame_header.rows4x4);
  }
  frame_scratch_buffer->tile_scratch_buffer_pool.Release(
      std::move(tile_scratch_buffer));
  return kStatusOk;
}

StatusCode DecoderImpl::DecodeTilesThreadedNonFrameParallel(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header,
    const Vector<std::unique_ptr<Tile>>& tiles,
    const Vector<ObuTileGroup>& tile_groups,
    const BlockParametersHolder& block_parameters_holder,
    FrameScratchBuffer* const frame_scratch_buffer,
    PostFilter* const post_filter,
    BlockingCounterWithStatus* const pending_tiles) {
  ThreadingStrategy& threading_strategy =
      frame_scratch_buffer->threading_strategy;
  const int num_workers = threading_strategy.tile_thread_count();
  BlockingCounterWithStatus pending_workers(num_workers);
  std::atomic<int> tile_counter(0);
  const int tile_count = static_cast<int>(tiles.size());
  bool tile_decoding_failed = false;
  // Submit tile decoding jobs to the thread pool.
  for (int i = 0; i < num_workers; ++i) {
    threading_strategy.tile_thread_pool()->Schedule([&tiles, tile_count,
                                                     &tile_counter,
                                                     &pending_workers,
                                                     &pending_tiles]() {
      bool failed = false;
      int index;
      while ((index = tile_counter.fetch_add(1, std::memory_order_relaxed)) <
             tile_count) {
        if (!failed) {
          const auto& tile_ptr = tiles[index];
          if (!tile_ptr->ParseAndDecode(/*is_main_thread=*/false)) {
            LIBGAV1_DLOG(ERROR, "Error decoding tile #%d", tile_ptr->number());
            failed = true;
          }
        } else {
          pending_tiles->Decrement(false);
        }
      }
      pending_workers.Decrement(!failed);
    });
  }
  // Have the current thread partake in tile decoding.
  int index;
  while ((index = tile_counter.fetch_add(1, std::memory_order_relaxed)) <
         tile_count) {
    if (!tile_decoding_failed) {
      const auto& tile_ptr = tiles[index];
      if (!tile_ptr->ParseAndDecode(/*is_main_thread=*/true)) {
        LIBGAV1_DLOG(ERROR, "Error decoding tile #%d", tile_ptr->number());
        tile_decoding_failed = true;
      }
    } else {
      pending_tiles->Decrement(false);
    }
  }
  // Wait until all the workers are done. This ensures that all the tiles have
  // been parsed.
  tile_decoding_failed |= !pending_workers.Wait();
  // Wait until all the tiles have been decoded.
  tile_decoding_failed |= !pending_tiles->Wait();
  if (tile_decoding_failed) return kStatusUnknownError;
  if (post_filter->DoDeblock() && kDeblockFilterBitMask) {
    frame_scratch_buffer->loop_filter_mask.Build(
        sequence_header, frame_header, tile_groups.front().start,
        tile_groups.back().end, block_parameters_holder,
        frame_scratch_buffer->inter_transform_sizes);
  }
  if (threading_strategy.post_filter_thread_pool() != nullptr) {
    post_filter->ApplyFilteringThreaded();
  }
  return kStatusOk;
}

StatusCode DecoderImpl::DecodeTilesFrameParallel(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header,
    const Vector<std::unique_ptr<Tile>>& tiles,
    const SymbolDecoderContext& saved_symbol_decoder_context,
    const SegmentationMap* const prev_segment_ids,
    FrameScratchBuffer* const frame_scratch_buffer,
    PostFilter* const post_filter, RefCountedBuffer* const current_frame) {
  // Parse the frame.
  for (const auto& tile : tiles) {
    if (!tile->Parse()) {
      LIBGAV1_DLOG(ERROR, "Failed to parse tile number: %d\n", tile->number());
      return kStatusUnknownError;
    }
  }
  if (frame_header.enable_frame_end_update_cdf) {
    frame_scratch_buffer->symbol_decoder_context = saved_symbol_decoder_context;
  }
  current_frame->SetFrameContext(frame_scratch_buffer->symbol_decoder_context);
  SetCurrentFrameSegmentationMap(frame_header, prev_segment_ids, current_frame);
  // Mark frame as parsed.
  current_frame->SetFrameState(kFrameStateParsed);
  std::unique_ptr<TileScratchBuffer> tile_scratch_buffer =
      frame_scratch_buffer->tile_scratch_buffer_pool.Get();
  if (tile_scratch_buffer == nullptr) {
    return kStatusOutOfMemory;
  }
  const int block_width4x4 = sequence_header.use_128x128_superblock ? 32 : 16;
  // Decode in superblock row order (inter prediction in the Tile class will
  // block until the required superblocks in the reference frame are decoded).
  for (int row4x4 = 0; row4x4 < frame_header.rows4x4;
       row4x4 += block_width4x4) {
    for (const auto& tile_ptr : tiles) {
      if (!tile_ptr->ProcessSuperBlockRow<kProcessingModeDecodeOnly, false>(
              row4x4, tile_scratch_buffer.get())) {
        LIBGAV1_DLOG(ERROR, "Failed to decode tile number: %d\n",
                     tile_ptr->number());
        return kStatusUnknownError;
      }
    }
    const int progress_row = post_filter->ApplyFilteringForOneSuperBlockRow(
        row4x4, block_width4x4,
        row4x4 + block_width4x4 >= frame_header.rows4x4);
    if (progress_row >= 0) {
      current_frame->SetProgress(progress_row);
    }
  }
  // Mark frame as decoded (we no longer care about row-level progress since the
  // entire frame has been decoded).
  current_frame->SetFrameState(kFrameStateDecoded);
  frame_scratch_buffer->tile_scratch_buffer_pool.Release(
      std::move(tile_scratch_buffer));
  return kStatusOk;
}

void DecoderImpl::SetCurrentFrameSegmentationMap(
    const ObuFrameHeader& frame_header, const SegmentationMap* prev_segment_ids,
    RefCountedBuffer* const current_frame) {
  if (!frame_header.segmentation.enabled) {
    // All segment_id's are 0.
    current_frame->segmentation_map()->Clear();
  } else if (!frame_header.segmentation.update_map) {
    // Copy from prev_segment_ids.
    if (prev_segment_ids == nullptr) {
      // Treat a null prev_segment_ids pointer as if it pointed to a
      // segmentation map containing all 0s.
      current_frame->segmentation_map()->Clear();
    } else {
      current_frame->segmentation_map()->CopyFrom(*prev_segment_ids);
    }
  }
}

StatusCode DecoderImpl::ApplyFilmGrain(
    const ObuSequenceHeader& sequence_header,
    const ObuFrameHeader& frame_header,
    const RefCountedBufferPtr& displayable_frame,
    RefCountedBufferPtr* film_grain_frame, ThreadPool* thread_pool) {
  if (!sequence_header.film_grain_params_present ||
      !displayable_frame->film_grain_params().apply_grain ||
      (settings_.post_filter_mask & 0x10) == 0) {
    *film_grain_frame = displayable_frame;
    return kStatusOk;
  }
  if (!frame_header.show_existing_frame &&
      frame_header.refresh_frame_flags == 0 &&
      // TODO(vigneshv): In frame parallel mode, we never do film grain in
      // place. Revisit this and see if this constraint need to be enforced.
      !IsFrameParallel()) {
    // If show_existing_frame is true, then the current frame is a previously
    // saved reference frame. If refresh_frame_flags is nonzero, then the
    // state_.UpdateReferenceFrames() call above has saved the current frame as
    // a reference frame. Therefore, if both of these conditions are false, then
    // the current frame is not saved as a reference frame. displayable_frame
    // should hold the only reference to the current frame.
    assert(displayable_frame.use_count() == 1);
    // Add film grain noise in place.
    *film_grain_frame = displayable_frame;
  } else {
    *film_grain_frame = buffer_pool_.GetFreeBuffer();
    if (*film_grain_frame == nullptr) {
      LIBGAV1_DLOG(ERROR,
                   "Could not get film_grain_frame from the buffer pool.");
      return kStatusResourceExhausted;
    }
    if (!(*film_grain_frame)
             ->Realloc(displayable_frame->buffer()->bitdepth(),
                       displayable_frame->buffer()->is_monochrome(),
                       displayable_frame->upscaled_width(),
                       displayable_frame->frame_height(),
                       displayable_frame->buffer()->subsampling_x(),
                       displayable_frame->buffer()->subsampling_y(),
                       kBorderPixelsFilmGrain, kBorderPixelsFilmGrain,
                       kBorderPixelsFilmGrain, kBorderPixelsFilmGrain)) {
      LIBGAV1_DLOG(ERROR, "film_grain_frame->Realloc() failed.");
      return kStatusOutOfMemory;
    }
    (*film_grain_frame)
        ->set_chroma_sample_position(
            displayable_frame->chroma_sample_position());
  }
  const bool color_matrix_is_identity =
      sequence_header.color_config.matrix_coefficients ==
      kMatrixCoefficientsIdentity;
  assert(displayable_frame->buffer()->stride(kPlaneU) ==
         displayable_frame->buffer()->stride(kPlaneV));
  const int input_stride_uv = displayable_frame->buffer()->stride(kPlaneU);
  assert((*film_grain_frame)->buffer()->stride(kPlaneU) ==
         (*film_grain_frame)->buffer()->stride(kPlaneV));
  const int output_stride_uv = (*film_grain_frame)->buffer()->stride(kPlaneU);
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (displayable_frame->buffer()->bitdepth() > 8) {
    FilmGrain<10> film_grain(displayable_frame->film_grain_params(),
                             displayable_frame->buffer()->is_monochrome(),
                             color_matrix_is_identity,
                             displayable_frame->buffer()->subsampling_x(),
                             displayable_frame->buffer()->subsampling_y(),
                             displayable_frame->upscaled_width(),
                             displayable_frame->frame_height(), thread_pool);
    if (!film_grain.AddNoise(
            displayable_frame->buffer()->data(kPlaneY),
            displayable_frame->buffer()->stride(kPlaneY),
            displayable_frame->buffer()->data(kPlaneU),
            displayable_frame->buffer()->data(kPlaneV), input_stride_uv,
            (*film_grain_frame)->buffer()->data(kPlaneY),
            (*film_grain_frame)->buffer()->stride(kPlaneY),
            (*film_grain_frame)->buffer()->data(kPlaneU),
            (*film_grain_frame)->buffer()->data(kPlaneV), output_stride_uv)) {
      LIBGAV1_DLOG(ERROR, "film_grain.AddNoise() failed.");
      return kStatusOutOfMemory;
    }
    return kStatusOk;
  }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
  FilmGrain<8> film_grain(displayable_frame->film_grain_params(),
                          displayable_frame->buffer()->is_monochrome(),
                          color_matrix_is_identity,
                          displayable_frame->buffer()->subsampling_x(),
                          displayable_frame->buffer()->subsampling_y(),
                          displayable_frame->upscaled_width(),
                          displayable_frame->frame_height(), thread_pool);
  if (!film_grain.AddNoise(
          displayable_frame->buffer()->data(kPlaneY),
          displayable_frame->buffer()->stride(kPlaneY),
          displayable_frame->buffer()->data(kPlaneU),
          displayable_frame->buffer()->data(kPlaneV), input_stride_uv,
          (*film_grain_frame)->buffer()->data(kPlaneY),
          (*film_grain_frame)->buffer()->stride(kPlaneY),
          (*film_grain_frame)->buffer()->data(kPlaneU),
          (*film_grain_frame)->buffer()->data(kPlaneV), output_stride_uv)) {
    LIBGAV1_DLOG(ERROR, "film_grain.AddNoise() failed.");
    return kStatusOutOfMemory;
  }
  return kStatusOk;
}

bool DecoderImpl::IsNewSequenceHeader(const ObuParser& obu) {
  if (std::find_if(obu.obu_headers().begin(), obu.obu_headers().end(),
                   [](const ObuHeader& obu_header) {
                     return obu_header.type == kObuSequenceHeader;
                   }) == obu.obu_headers().end()) {
    return false;
  }
  const ObuSequenceHeader sequence_header = obu.sequence_header();
  const bool sequence_header_changed =
      !has_sequence_header_ ||
      sequence_header_.color_config.bitdepth !=
          sequence_header.color_config.bitdepth ||
      sequence_header_.color_config.is_monochrome !=
          sequence_header.color_config.is_monochrome ||
      sequence_header_.color_config.subsampling_x !=
          sequence_header.color_config.subsampling_x ||
      sequence_header_.color_config.subsampling_y !=
          sequence_header.color_config.subsampling_y ||
      sequence_header_.max_frame_width != sequence_header.max_frame_width ||
      sequence_header_.max_frame_height != sequence_header.max_frame_height;
  sequence_header_ = sequence_header;
  has_sequence_header_ = true;
  return sequence_header_changed;
}

}  // namespace libgav1
