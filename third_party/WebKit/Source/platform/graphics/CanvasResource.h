// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"

#ifndef CanvasResource_h
#define CanvasResource_h

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace blink {

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
class PLATFORM_EXPORT CanvasResource {
 public:
  virtual ~CanvasResource();
  virtual void Abandon() = 0;
  virtual bool IsRecycleable() const = 0;
  virtual bool IsValid() const = 0;
  virtual GLuint TextureId() const = 0;
  gpu::gles2::GLES2Interface* ContextGL() const;
  const gpu::Mailbox& GpuMailbox();
  void SetSyncTokenForRelease(const gpu::SyncToken&);
  void WaitSyncTokenBeforeRelease();

 protected:
  CanvasResource(WeakPtr<WebGraphicsContext3DProviderWrapper>);

  gpu::Mailbox gpu_mailbox_;
  // Sync token that was provided when resource was released
  gpu::SyncToken sync_token_for_release_;
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

// Resource type for skia Bitmaps (RAM and texture backed)
class PLATFORM_EXPORT CanvasResource_Skia final : public CanvasResource {
 public:
  static std::unique_ptr<CanvasResource_Skia> Create(
      sk_sp<SkImage>,
      WeakPtr<WebGraphicsContext3DProviderWrapper>);
  virtual ~CanvasResource_Skia() { Abandon(); }

  // Not recyclable: Skia handles texture recycling internally and bitmaps are
  // cheap to allocate.
  bool IsRecycleable() const final { return false; }
  bool IsValid() const final;
  void Abandon() final;
  GLuint TextureId() const final;

 private:
  CanvasResource_Skia(sk_sp<SkImage>,
                      WeakPtr<WebGraphicsContext3DProviderWrapper>);

  sk_sp<SkImage> image_;
};

// Resource type for GpuMemoryBuffers
class PLATFORM_EXPORT CanvasResource_GpuMemoryBuffer final
    : public CanvasResource {
 public:
  static std::unique_ptr<CanvasResource_GpuMemoryBuffer> Create(
      const IntSize&,
      const CanvasColorParams&,
      WeakPtr<WebGraphicsContext3DProviderWrapper>);
  virtual ~CanvasResource_GpuMemoryBuffer() { Abandon(); }
  bool IsRecycleable() const final { return IsValid(); }
  bool IsValid() const { return context_provider_wrapper_ && image_id_; }
  void Abandon() final;
  GLuint TextureId() const final { return texture_id_; }

 private:
  CanvasResource_GpuMemoryBuffer(const IntSize&,
                                 const CanvasColorParams&,
                                 WeakPtr<WebGraphicsContext3DProviderWrapper>);

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  GLuint image_id_ = 0;
  GLuint texture_id_ = 0;
  CanvasColorParams color_params_;
};

}  // namespace blink

#endif  // CanvasResource_h
