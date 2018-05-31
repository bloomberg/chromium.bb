// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

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
class PLATFORM_EXPORT CanvasResource
    : public WTF::ThreadSafeRefCounted<CanvasResource> {
 public:
  virtual ~CanvasResource();
  virtual void Abandon() { TearDown(); }
  virtual bool IsRecycleable() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool IsValid() const = 0;
  virtual IntSize Size() const = 0;
  virtual const gpu::Mailbox& GetOrCreateGpuMailbox() = 0;
  virtual void Transfer() { NOTREACHED(); }
  virtual const gpu::SyncToken& GetSyncToken() = 0;
  bool PrepareTransferableResource(
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_callback);
  void WaitSyncToken(const gpu::SyncToken&);
  virtual scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) = 0;
  virtual scoped_refptr<CanvasResource> MakeUnaccelerated() = 0;
  virtual bool IsBitmap();
  virtual bool OriginClean() const {
    NOTREACHED();
    return false;
  }
  virtual scoped_refptr<StaticBitmapImage> Bitmap();
  virtual void CopyFromTexture(GLuint source_texture,
                               GLenum format,
                               GLenum type) {
    NOTREACHED();
  }

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>,
                 SkFilterQuality,
                 const CanvasColorParams&);
  virtual GLenum TextureTarget() const = 0;
  virtual bool IsOverlayCandidate() const { return false; }
  virtual bool HasGpuMailbox() const = 0;
  virtual void TearDown() = 0;
  gpu::gles2::GLES2Interface* ContextGL() const;
  GLenum GLFilter() const;
  GrContext* GetGrContext() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const = 0;
  void PrepareTransferableResourceCommon(
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_callback);
  SkFilterQuality FilterQuality() const { return filter_quality_; }
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void OnDestroy();

 private:
  // Sync token that was provided when resource was released
  gpu::SyncToken sync_token_for_release_;
  base::WeakPtr<CanvasResourceProvider> provider_;
  SkFilterQuality filter_quality_;
  CanvasColorParams color_params_;
  blink::PlatformThreadId thread_of_origin_;
#if DCHECK_IS_ON()
  bool did_call_on_destroy_ = false;
#endif
};

// Resource type for skia Bitmaps (RAM and texture backed)
class PLATFORM_EXPORT CanvasResourceBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceBitmap> Create(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      const CanvasColorParams&);
  ~CanvasResourceBitmap() override;

  // Not recyclable: Skia handles texture recycling internally and bitmaps are
  // cheap to allocate.
  bool IsRecycleable() const final { return false; }
  bool IsAccelerated() const final;
  bool IsValid() const final;
  IntSize Size() const final;
  bool IsBitmap() final;
  void Transfer() final;
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final;
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final;
  scoped_refptr<CanvasResource> MakeUnaccelerated() final;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::Mailbox& GetOrCreateGpuMailbox() override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken& GetSyncToken() override;

  CanvasResourceBitmap(scoped_refptr<StaticBitmapImage>,
                       base::WeakPtr<CanvasResourceProvider>,
                       SkFilterQuality,
                       const CanvasColorParams&);

  scoped_refptr<StaticBitmapImage> image_;
};

// Resource type for GpuMemoryBuffers
class PLATFORM_EXPORT CanvasResourceGpuMemoryBuffer final
    : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceGpuMemoryBuffer> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  ~CanvasResourceGpuMemoryBuffer() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return true; }
  bool IsValid() const override {
    return context_provider_wrapper_ && image_id_;
  }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    return base::WrapRefCounted(this);
  };
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  bool IsOverlayCandidate() const final { return true; }
  const gpu::Mailbox& GetOrCreateGpuMailbox() override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken& GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override;

  CanvasResourceGpuMemoryBuffer(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);

  gpu::Mailbox gpu_mailbox_;
  gpu::SyncToken sync_token_;
  bool mailbox_needs_new_sync_token_ = false;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  GLuint image_id_ = 0;
  GLuint texture_id_ = 0;
  CanvasColorParams color_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
