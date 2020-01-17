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

#include "src/buffer_pool.h"

#include <cassert>
#include <cstring>

#include "src/utils/common.h"
#include "src/utils/logging.h"

namespace libgav1 {

namespace {

// Copies the feature_enabled, feature_data, segment_id_pre_skip, and
// last_active_segment_id fields of Segmentation.
void CopySegmentationParameters(const Segmentation& from, Segmentation* to) {
  memcpy(to->feature_enabled, from.feature_enabled,
         sizeof(to->feature_enabled));
  memcpy(to->feature_data, from.feature_data, sizeof(to->feature_data));
  to->segment_id_pre_skip = from.segment_id_pre_skip;
  to->last_active_segment_id = from.last_active_segment_id;
}

}  // namespace

RefCountedBuffer::RefCountedBuffer() = default;

RefCountedBuffer::~RefCountedBuffer() = default;

bool RefCountedBuffer::Realloc(int bitdepth, bool is_monochrome, int width,
                               int height, int subsampling_x, int subsampling_y,
                               int left_border, int right_border,
                               int top_border, int bottom_border) {
  assert(!buffer_private_data_valid_);
  if (!yuv_buffer_.Realloc(
          bitdepth, is_monochrome, width, height, subsampling_x, subsampling_y,
          left_border, right_border, top_border, bottom_border,
          pool_->get_frame_buffer_, pool_->callback_private_data_,
          &buffer_private_data_)) {
    return false;
  }
  buffer_private_data_valid_ = true;
  return true;
}

bool RefCountedBuffer::SetFrameDimensions(const ObuFrameHeader& frame_header) {
  upscaled_width_ = frame_header.upscaled_width;
  frame_width_ = frame_header.width;
  frame_height_ = frame_header.height;
  render_width_ = frame_header.render_width;
  render_height_ = frame_header.render_height;
  rows4x4_ = frame_header.rows4x4;
  columns4x4_ = frame_header.columns4x4;
  const int rows4x4_half = DivideBy2(rows4x4_);
  const int columns4x4_half = DivideBy2(columns4x4_);
  if (!motion_field_reference_frame_.Reset(rows4x4_half, columns4x4_half,
                                           /*zero_initialize=*/false) ||
      !motion_field_mv_.Reset(rows4x4_half, columns4x4_half,
                              /*zero_initialize=*/false)) {
    return false;
  }
  return segmentation_map_.Allocate(rows4x4_, columns4x4_);
}

void RefCountedBuffer::SetGlobalMotions(
    const std::array<GlobalMotion, kNumReferenceFrameTypes>& global_motions) {
  for (int ref = kReferenceFrameLast; ref <= kReferenceFrameAlternate; ++ref) {
    static_assert(sizeof(global_motion_[ref].params) ==
                      sizeof(global_motions[ref].params),
                  "");
    memcpy(global_motion_[ref].params, global_motions[ref].params,
           sizeof(global_motion_[ref].params));
  }
}

void RefCountedBuffer::SetFrameContext(const SymbolDecoderContext& context) {
  frame_context_ = context;
  frame_context_.ResetIntraFrameYModeCdf();
  frame_context_.ResetCounters();
}

void RefCountedBuffer::GetSegmentationParameters(
    Segmentation* segmentation) const {
  CopySegmentationParameters(/*from=*/segmentation_, /*to=*/segmentation);
}

void RefCountedBuffer::SetSegmentationParameters(
    const Segmentation& segmentation) {
  CopySegmentationParameters(/*from=*/segmentation, /*to=*/&segmentation_);
}

void RefCountedBuffer::SetBufferPool(BufferPool* pool) { pool_ = pool; }

void RefCountedBuffer::ReturnToBufferPool(RefCountedBuffer* ptr) {
  ptr->pool_->ReturnUnusedBuffer(ptr);
}

// static
constexpr int BufferPool::kNumBuffers;

BufferPool::BufferPool(
    FrameBufferSizeChangedCallback on_frame_buffer_size_changed,
    GetFrameBufferCallback2 get_frame_buffer,
    ReleaseFrameBufferCallback2 release_frame_buffer,
    void* callback_private_data) {
  if (get_frame_buffer != nullptr) {
    assert(on_frame_buffer_size_changed != nullptr);
    assert(release_frame_buffer != nullptr);
    on_frame_buffer_size_changed_ = on_frame_buffer_size_changed;
    get_frame_buffer_ = get_frame_buffer;
    release_frame_buffer_ = release_frame_buffer;
    callback_private_data_ = callback_private_data;
  } else {
    internal_frame_buffers_ = InternalFrameBufferList::Create(kNumBuffers);
    // OnInternalFrameBufferSizeChanged and GetInternalFrameBuffer check
    // whether their private_data argument is null, so we don't need to check
    // whether internal_frame_buffers_ is null here.
    on_frame_buffer_size_changed_ = OnInternalFrameBufferSizeChanged;
    get_frame_buffer_ = GetInternalFrameBuffer;
    release_frame_buffer_ = ReleaseInternalFrameBuffer;
    callback_private_data_ = internal_frame_buffers_.get();
  }
  for (RefCountedBuffer& buffer : buffers_) {
    buffer.SetBufferPool(this);
  }
}

BufferPool::~BufferPool() {
  for (const RefCountedBuffer& buffer : buffers_) {
    if (buffer.in_use_) {
      assert(false && "RefCountedBuffer still in use at destruction time.");
      LIBGAV1_DLOG(ERROR, "RefCountedBuffer still in use at destruction time.");
    }
  }
}

bool BufferPool::OnFrameBufferSizeChanged(int bitdepth, bool is_monochrome,
                                          int8_t subsampling_x,
                                          int8_t subsampling_y, int width,
                                          int height, int left_border,
                                          int right_border, int top_border,
                                          int bottom_border) {
  const int aligned_width = Align(width, 8);
  const int aligned_height = Align(height, 8);
  return on_frame_buffer_size_changed_(
             callback_private_data_, bitdepth, is_monochrome, subsampling_x,
             subsampling_y, aligned_width, aligned_height, left_border,
             right_border, top_border, bottom_border,
             /*stride_alignment=*/16) == 0;
}

RefCountedBufferPtr BufferPool::GetFreeBuffer() {
  for (RefCountedBuffer& buffer : buffers_) {
    if (!buffer.in_use_) {
      buffer.in_use_ = true;
      return RefCountedBufferPtr(&buffer, RefCountedBuffer::ReturnToBufferPool);
    }
  }

  // We should never run out of free buffers. If we reach here, there is a
  // reference leak.
  return RefCountedBufferPtr();
}

void BufferPool::ReturnUnusedBuffer(RefCountedBuffer* buffer) {
  assert(buffer->in_use_);
  buffer->in_use_ = false;
  if (buffer->buffer_private_data_valid_) {
    release_frame_buffer_(callback_private_data_, buffer->buffer_private_data_);
    buffer->buffer_private_data_valid_ = false;
  }
}

}  // namespace libgav1
