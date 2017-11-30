// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasResourceProvider_h
#define CanvasResourceProvider_h

#include "base/memory/scoped_refptr.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/WeakPtr.h"
#include "third_party/khronos/GLES2/gl2.h"

class GrContext;
class SkCanvas;
class SkSurface;

namespace cc {

class PaintCanvas;
}

namespace gpu {
namespace gles2 {

class GLES2Interface;

}  // namespace gles2
}  // namespace gpu

namespace viz {

class SingleReleaseCallback;
struct TransferableResource;

}  // namespace viz

namespace blink {

class CanvasResource;
class StaticBitmapImage;
class WebGraphicsContext3DProviderWrapper;

// CanvasResourceProvider
//==============================================================================
//
// This is an abstract base class that encapsulates a drawable graphics
// resource.  Subclasses manage specific resource types (Gpu Textures,
// GpuMemoryBuffer, Bitmap in RAM). CanvasResourceProvider serves as an
// abstraction layer for these resource types. It is designed to serve
// the needs of Canvas2DLayerBridge, but can also be used as a general purpose
// provider of drawable surfaces for 2D rendering with skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

class PLATFORM_EXPORT CanvasResourceProvider {
  WTF_MAKE_NONCOPYABLE(CanvasResourceProvider);

 public:
  enum ResourceUsage {
    kSoftwareResourceUsage,
    kSoftwareCompositedResourceUsage,
    kAcceleratedResourceUsage,
    kAcceleratedCompositedResourceUsage,
  };

  static std::unique_ptr<CanvasResourceProvider> Create(
      const IntSize&,
      unsigned msaa_sample_count,
      const CanvasColorParams&,
      ResourceUsage,
      WeakPtr<WebGraphicsContext3DProviderWrapper> = nullptr);

  cc::PaintCanvas* Canvas();
  void FlushSkia() const;
  const CanvasColorParams& ColorParams() const { return color_params_; }
  scoped_refptr<StaticBitmapImage> Snapshot();
  void SetFilterQuality(SkFilterQuality quality) { filter_quality_ = quality; }
  bool PrepareTransferableResource(
      viz::TransferableResource*,
      std::unique_ptr<viz::SingleReleaseCallback>*);
  const IntSize& Size() const { return size_; }
  virtual bool IsValid() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool CanPrepareTransferableResource() const = 0;
  uint32_t ContentUniqueID() const;
  void ClearRecycledResources();
  void RecycleResource(scoped_refptr<CanvasResource>);
  void SetResourceRecyclingEnabled(bool);
  SkSurface* GetSkSurface() const;
  bool IsGpuContextLost() const;
  virtual ~CanvasResourceProvider();

 protected:
  gpu::gles2::GLES2Interface* ContextGL() const;
  GrContext* GetGrContext() const;
  WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper() {
    return context_provider_wrapper_;
  }
  GLenum GetGLFilter() const;
  bool UseNearestNeighbor() const;
  void ResetSkiaTextureBinding() const;
  scoped_refptr<CanvasResource> NewOrRecycledResource();

  // Called by subclasses when the backing resource has changed and resources
  // are not managed by skia, signaling that a new surface needs to be created.
  void InvalidateSurface();

  CanvasResourceProvider(const IntSize&,
                         const CanvasColorParams&,
                         WeakPtr<WebGraphicsContext3DProviderWrapper>);

 private:
  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;
  virtual scoped_refptr<CanvasResource> CreateResource();
  virtual scoped_refptr<CanvasResource> DoPrepareTransferableResource(
      viz::TransferableResource* out_resource) = 0;

  WeakPtrFactory<CanvasResourceProvider> weak_ptr_factory_;
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  IntSize size_;
  CanvasColorParams color_params_;
  std::unique_ptr<cc::PaintCanvas> canvas_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init
  std::unique_ptr<SkCanvas> xform_canvas_;
  WTF::Vector<scoped_refptr<CanvasResource>> recycled_resources_;
  SkFilterQuality filter_quality_;
  bool resource_recycling_enabled_ = true;
};

}  // namespace blink

#endif
