// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/GpuMemoryBufferImageCopy.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

GpuMemoryBufferImageCopy::GpuMemoryBufferImageCopy(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl) {
  gl_->GenFramebuffers(1, &draw_frame_buffer_);
}

GpuMemoryBufferImageCopy::~GpuMemoryBufferImageCopy() {
  gl_->DeleteFramebuffers(1, &draw_frame_buffer_);
}

bool GpuMemoryBufferImageCopy::EnsureMemoryBuffer(int width, int height) {
  // Create a new memorybuffer if the size has changed, or we don't have one.
  if (last_width_ != width || last_height_ != height || !gpu_memory_buffer_) {
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
        Platform::Current()->GetGpuMemoryBufferManager();
    if (!gpu_memory_buffer_manager)
      return false;

    gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
        gfx::Size(width, height), gfx::BufferFormat::RGBA_8888,
        gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_)
      return false;

    last_width_ = width;
    last_height_ = height;
  }
  return true;
}

gfx::GpuMemoryBuffer* GpuMemoryBufferImageCopy::CopyImage(Image* image) {
  if (!image)
    return nullptr;

  TRACE_EVENT0("gpu", "GpuMemoryBufferImageCopy::CopyImage");

  int width = image->width();
  int height = image->height();
  if (!EnsureMemoryBuffer(width, height))
    return nullptr;

  // Bind the write framebuffer to our memory buffer.
  GLuint image_id = gl_->CreateImageCHROMIUM(
      gpu_memory_buffer_->AsClientBuffer(), width, height, GL_RGBA);
  if (!image_id)
    return nullptr;
  GLuint dest_texture_id;
  gl_->GenTextures(1, &dest_texture_id);
  GLenum target = GL_TEXTURE_2D;
  {
    gl_->BindTexture(target, dest_texture_id);
    gl_->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->BindTexImage2DCHROMIUM(target, image_id);
  }
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer_);
  gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            dest_texture_id, 0);

  // Bind the read framebuffer to our image.
  StaticBitmapImage* static_image = static_cast<StaticBitmapImage*>(image);
  static_image->EnsureMailbox(kOrderingBarrier);
  auto mailbox = static_image->GetMailbox();
  auto sync_token = static_image->GetSyncToken();
  // Not strictly necessary since we are on the same context, but keeping
  // for cleanliness and in case we ever move off the same context.
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetData());
  GLuint source_texture_id =
      gl_->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  GLuint read_frame_buffer;
  gl_->GenFramebuffers(1, &read_frame_buffer);
  gl_->BindFramebuffer(GL_READ_FRAMEBUFFER, read_frame_buffer);
  gl_->FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            source_texture_id, 0);

  // Copy the read framebuffer to the draw framebuffer.
  gl_->BlitFramebufferCHROMIUM(0, 0, width, height, 0, 0, width, height,
                               GL_COLOR_BUFFER_BIT, GL_LINEAR);

  // Cleanup the read framebuffer, associated image and texture.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_->DeleteFramebuffers(1, &read_frame_buffer);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  gl_->DeleteTextures(1, &source_texture_id);

  // Cleanup the draw framebuffer, associated image and texture.
  gl_->BindTexture(target, dest_texture_id);
  gl_->ReleaseTexImage2DCHROMIUM(target, image_id);
  gl_->DestroyImageCHROMIUM(image_id);
  gl_->DeleteTextures(1, &dest_texture_id);
  gl_->BindTexture(target, 0);

  gl_->ShallowFlushCHROMIUM();
  return gpu_memory_buffer_.get();
}

}  // namespace blink
