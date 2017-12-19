// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/wtf/RefCounted.h"

#ifndef CanvasResource_h
#define CanvasResource_h

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace viz {

class SingleReleaseCallback;
struct TransferableResource;

}  // namespace viz

namespace blink {

class CanvasResourceProvider;
class StaticBitmapImage;

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
class PLATFORM_EXPORT CanvasResource : public WTF::RefCounted<CanvasResource> {
 public:
  virtual ~CanvasResource();
  virtual void Abandon() = 0;
  virtual bool IsRecycleable() const = 0;
  virtual bool IsValid() const = 0;
  virtual IntSize Size() const = 0;
  virtual const gpu::Mailbox& GetOrCreateGpuMailbox() = 0;
  bool PrepareTransferableResource(
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_callback);
  void SetSyncTokenForRelease(const gpu::SyncToken&);
  void WaitSyncTokenBeforeRelease();
  virtual void CopyFromTexture(GLuint source_texture,
                               GLenum format,
                               GLenum type) {
    NOTREACHED();
  }

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>, SkFilterQuality);
  virtual GLenum TextureTarget() const = 0;
  virtual bool IsOverlayCandidate() const { return false; }
  virtual const gpu::SyncToken& GetSyncToken() const = 0;
  virtual bool HasGpuMailbox() const = 0;
  gpu::gles2::GLES2Interface* ContextGL() const;
  GLenum GLFilter() const;
  GrContext* GetGrContext() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const = 0;
  void PrepareTransferableResourceCommon(
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_callback);

 private:
  // Sync token that was provided when resource was released
  gpu::SyncToken sync_token_for_release_;
  base::WeakPtr<CanvasResourceProvider> provider_;
  SkFilterQuality filter_quality_;
};

// Resource type for skia Bitmaps (RAM and texture backed)
class PLATFORM_EXPORT CanvasResource_Bitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResource_Bitmap> Create(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  virtual ~CanvasResource_Bitmap() { Abandon(); }

  // Not recyclable: Skia handles texture recycling internally and bitmaps are
  // cheap to allocate.
  bool IsRecycleable() const final { return false; }
  bool IsValid() const final;
  void Abandon() final { TearDown(); }
  IntSize Size() const final;

 private:
  void TearDown();
  GLenum TextureTarget() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::Mailbox& GetOrCreateGpuMailbox() override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken& GetSyncToken() const;

  CanvasResource_Bitmap(scoped_refptr<StaticBitmapImage>,
                        base::WeakPtr<CanvasResourceProvider>,
                        SkFilterQuality);

  scoped_refptr<StaticBitmapImage> image_;
};

// Resource type for GpuMemoryBuffers
class PLATFORM_EXPORT CanvasResource_GpuMemoryBuffer final
    : public CanvasResource {
 public:
  static scoped_refptr<CanvasResource_GpuMemoryBuffer> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  virtual ~CanvasResource_GpuMemoryBuffer();
  bool IsRecycleable() const final { return IsValid(); }
  bool IsValid() const { return context_provider_wrapper_ && image_id_; }
  void Abandon() final { TearDown(); }
  IntSize Size() const final;

 private:
  void TearDown();
  GLenum TextureTarget() const final;
  bool IsOverlayCandidate() const final { return true; }
  const gpu::Mailbox& GetOrCreateGpuMailbox() override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken& GetSyncToken() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override;

  CanvasResource_GpuMemoryBuffer(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);

  gpu::Mailbox gpu_mailbox_;
  gpu::SyncToken sync_token_;
  bool mailbox_needs_new_sync_token_ = true;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  GLuint image_id_ = 0;
  GLuint texture_id_ = 0;
  CanvasColorParams color_params_;
};

}  // namespace blink

#endif  // CanvasResource_h
