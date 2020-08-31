// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H_
#define MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H_

#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// A class that holds CodecOutputBuffer and renders it to TextureOwner or
// overlay as necessary. Unit tests for this class are part of CodecImage unit
// tests.
class MEDIA_GPU_EXPORT CodecOutputBufferRenderer {
 public:
  using BindingsMode = gpu::StreamTextureSharedImageInterface::BindingsMode;
  // Whether RenderToTextureOwnerBackBuffer may block or not.
  enum class BlockingMode { kForbidBlocking, kAllowBlocking };

  CodecOutputBufferRenderer(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator);
  ~CodecOutputBufferRenderer();

  CodecOutputBufferRenderer(const CodecOutputBufferRenderer&) = delete;
  CodecOutputBufferRenderer& operator=(const CodecOutputBufferRenderer&) =
      delete;

  // Renders this image to the overlay. Returns true if the buffer is in the
  // overlay front buffer. Returns false if the buffer was invalidated.
  bool RenderToOverlay();

  // Renders this image to the texture owner front buffer by first rendering
  // it to the back buffer if it's not already there, and then waiting for the
  // frame available event before calling UpdateTexImage().
  bool RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode);

  // Renders this image to the front buffer of its backing surface.
  // Returns true if the buffer is in the front buffer. Returns false if the
  // buffer was invalidated. After an image is invalidated it's no longer
  // possible to render it.
  bool RenderToFrontBuffer();

  // Renders this image to the back buffer of its texture owner. Only valid if
  // is_texture_owner_backed(). Returns true if the buffer is in the back
  // buffer. Returns false if the buffer was invalidated.
  // |blocking_mode| indicates whether this should (a) wait for any previously
  // pending rendered frame before rendering this one, or (b) fail if a wait
  // is required.
  bool RenderToTextureOwnerBackBuffer(
      BlockingMode blocking_mode = BlockingMode::kAllowBlocking);

  // Whether the codec buffer has been rendered to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return phase_ == Phase::kInFrontBuffer;
  }

  gfx::Size size() const { return output_buffer_->size(); }

  bool was_tex_image_bound() const { return was_tex_image_bound_; }

  scoped_refptr<gpu::TextureOwner> texture_owner() const {
    return codec_buffer_wait_coordinator_
               ? codec_buffer_wait_coordinator_->texture_owner()
               : nullptr;
  }

  CodecOutputBuffer* get_codec_output_buffer_for_testing() const {
    return output_buffer_.get();
  }

 private:
  // The lifecycle phases of an buffer.
  // The only possible transitions are from left to right. Both
  // kInFrontBuffer and kInvalidated are terminal.
  enum class Phase { kInCodec, kInBackBuffer, kInFrontBuffer, kInvalidated };

  void EnsureBoundIfNeeded(BindingsMode mode);

  // The phase of the image buffer's lifecycle.
  Phase phase_ = Phase::kInCodec;

  // The buffer backing this image.
  std::unique_ptr<CodecOutputBuffer> output_buffer_;

  // The CodecBufferWaitCoordinator that |output_buffer_| will be rendered to.
  // Or null, if this image is backed by an overlay.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  bool was_tex_image_bound_ = false;
};

}  // namespace media
#endif  // MEDIA_GPU_ANDROID_CODEC_OUTPUT_BUFFER_RENDERER_H
