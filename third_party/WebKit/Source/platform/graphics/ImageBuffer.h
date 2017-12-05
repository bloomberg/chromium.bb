/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/Uint8ClampedArray.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace WTF {

class ArrayBufferContents;

}  // namespace WTF

namespace blink {

class DrawingBuffer;
class GraphicsContext;
class IntPoint;
class IntRect;

class PLATFORM_EXPORT ImageBuffer {
  WTF_MAKE_NONCOPYABLE(ImageBuffer);
  USING_FAST_MALLOC(ImageBuffer);

 public:
  static std::unique_ptr<ImageBuffer> Create(
      const IntSize&,
      ImageInitializationMode = kInitializeImagePixels,
      const CanvasColorParams& = CanvasColorParams());
  static std::unique_ptr<ImageBuffer> Create(
      std::unique_ptr<ImageBufferSurface>);

  virtual ~ImageBuffer();

  static bool CanCreateImageBuffer(const IntSize&);
  const IntSize& Size() const { return surface_->Size(); }
  bool IsAccelerated() const { return surface_->IsAccelerated(); }
  void PrepareSurfaceForPaintingIfNeeded() {
    surface_->PrepareSurfaceForPaintingIfNeeded();
  }
  bool IsSurfaceValid() const;
  bool RestoreSurface() const;
  void DidDraw(const FloatRect&) const;
  bool WasDrawnToAfterSnapshot() const {
    return snapshot_state_ == kDrawnToAfterSnapshot;
  }

  void SetFilterQuality(SkFilterQuality filter_quality) {
    surface_->SetFilterQuality(filter_quality);
  }
  void SetIsHidden(bool hidden) { surface_->SetIsHidden(hidden); }

  PaintCanvas* Canvas() const;
  void DisableDeferral(DisableDeferralReason) const;

  // Called at the end of a task that rendered a whole frame
  void FinalizeFrame();
  void DoPaintInvalidation(const FloatRect& dirty_rect);

  void WillOverwriteCanvas() { surface_->WillOverwriteCanvas(); }

  bool GetImageData(const IntRect&,
                    WTF::ArrayBufferContents&,
                    bool* is_gpu_readback_invoked = nullptr) const;

  void PutByteArray(const unsigned char* source,
                    const IntSize& source_size,
                    const IntRect& source_rect,
                    const IntPoint& dest_point);

  WebLayer* PlatformLayer() const;

  // Destroys the TEXTURE_2D binding for the active texture unit of the passed
  // context. Assumes the destination texture has already been allocated.
  bool CopyToPlatformTexture(SnapshotReason,
                             gpu::gles2::GLES2Interface*,
                             GLenum target,
                             GLuint texture,
                             bool premultiply_alpha,
                             bool flip_y,
                             const IntPoint& dest_point,
                             const IntRect& source_sub_rectangle);

  bool CopyRenderingResultsFromDrawingBuffer(DrawingBuffer*,
                                             SourceDrawingBuffer);

  scoped_refptr<StaticBitmapImage> NewImageSnapshot(
      AccelerationHint = kPreferNoAcceleration,
      SnapshotReason = kSnapshotReasonUnknown) const;

  sk_sp<PaintRecord> GetRecord() { return surface_->GetRecord(); }

  void Draw(GraphicsContext&, const FloatRect&, const FloatRect*, SkBlendMode);

  void SetSurface(std::unique_ptr<ImageBufferSurface>);

  // TODO(xlai): This function is an intermediate step making canvas element
  // have reference to Canvas2DLayerBridge. See crbug.com/776806.
  void OnCanvasDisposed();

  base::WeakPtrFactory<ImageBuffer> weak_ptr_factory_;

 protected:
  ImageBuffer(std::unique_ptr<ImageBufferSurface>);

 private:
  enum SnapshotState {
    kInitialSnapshotState,
    kDidAcquireSnapshot,
    kDrawnToAfterSnapshot,
  };
  mutable SnapshotState snapshot_state_;
  std::unique_ptr<ImageBufferSurface> surface_;
};

class ImageDataBuffer {
 public:
  ImageDataBuffer(const IntSize& size, const unsigned char* data)
      : data_(data), uses_pixmap_(false), size_(size) {}
  ImageDataBuffer(const SkPixmap& pixmap)
      : pixmap_(pixmap),
        uses_pixmap_(true),
        size_(IntSize(pixmap.width(), pixmap.height())) {}
  PLATFORM_EXPORT ImageDataBuffer(scoped_refptr<StaticBitmapImage>);

  String PLATFORM_EXPORT ToDataURL(const String& mime_type,
                                   const double& quality) const;
  bool PLATFORM_EXPORT EncodeImage(const String& mime_type,
                                   const double& quality,
                                   Vector<unsigned char>* encoded_image) const;
  const unsigned char* Pixels() const;
  const IntSize& size() const { return size_; }
  int Height() const { return size_.Height(); }
  int Width() const { return size_.Width(); }

 private:
  const unsigned char* data_;
  SkPixmap pixmap_;
  bool uses_pixmap_ = false;
  IntSize size_;
  scoped_refptr<StaticBitmapImage> image_bitmap_;
};

}  // namespace blink

#endif  // ImageBuffer_h
