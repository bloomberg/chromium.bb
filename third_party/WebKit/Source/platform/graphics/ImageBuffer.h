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
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"
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
class ImageBufferClient;
class IntPoint;
class IntRect;

enum Multiply { kPremultiplied, kUnmultiplied };

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

  void SetClient(ImageBufferClient* client) { client_ = client; }

  static bool CanCreateImageBuffer(const IntSize&);
  const IntSize& Size() const { return surface_->Size(); }
  bool IsAccelerated() const { return surface_->IsAccelerated(); }
  bool IsRecording() const { return surface_->IsRecording(); }
  void SetHasExpensiveOp() { surface_->SetHasExpensiveOp(); }
  bool IsExpensiveToPaint() const { return surface_->IsExpensiveToPaint(); }
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

  // Called by subclasses of ImageBufferSurface to install a new canvas object.
  // Virtual for mocking
  virtual void ResetCanvas(PaintCanvas*) const;

  PaintCanvas* Canvas() const;
  void DisableDeferral(DisableDeferralReason) const;

  // Called at the end of a task that rendered a whole frame
  void FinalizeFrame();
  void DoPaintInvalidation(const FloatRect& dirty_rect);

  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);

  void WillOverwriteCanvas() { surface_->WillOverwriteCanvas(); }

  bool GetImageData(Multiply, const IntRect&, WTF::ArrayBufferContents&) const;

  void PutByteArray(Multiply,
                    const unsigned char* source,
                    const IntSize& source_size,
                    const IntRect& source_rect,
                    const IntPoint& dest_point);

  AffineTransform BaseTransform() const { return AffineTransform(); }
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

  void Flush(FlushReason);     // Process deferred draw commands immediately.
  void FlushGpu(FlushReason);  // Like flush(), but flushes all the way down to
                               // the GPU context if the surface is accelerated.

  void NotifySurfaceInvalid();

  RefPtr<StaticBitmapImage> NewImageSnapshot(
      AccelerationHint = kPreferNoAcceleration,
      SnapshotReason = kSnapshotReasonUnknown) const;

  sk_sp<PaintRecord> GetRecord() { return surface_->GetRecord(); }

  void Draw(GraphicsContext&, const FloatRect&, const FloatRect*, SkBlendMode);

  void UpdateGPUMemoryUsage() const;
  static intptr_t GetGlobalGPUMemoryUsage() { return global_gpu_memory_usage_; }
  static unsigned GetGlobalAcceleratedImageBufferCount() {
    return global_accelerated_image_buffer_count_;
  }
  intptr_t GetGPUMemoryUsage() { return gpu_memory_usage_; }

  void DisableAcceleration();
  void SetSurface(std::unique_ptr<ImageBufferSurface>);

  void SetNeedsCompositingUpdate();

  WeakPtrFactory<ImageBuffer> weak_ptr_factory_;

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
  ImageBufferClient* client_;

  mutable bool gpu_readback_invoked_in_current_frame_;
  int gpu_readback_successive_frames_;

  mutable intptr_t gpu_memory_usage_;
  static intptr_t global_gpu_memory_usage_;
  static unsigned global_accelerated_image_buffer_count_;
};

struct ImageDataBuffer {
  STACK_ALLOCATED();
  ImageDataBuffer(const IntSize& size, const unsigned char* data)
      : data_(data), size_(size) {}
  String PLATFORM_EXPORT ToDataURL(const String& mime_type,
                                   const double& quality) const;
  bool PLATFORM_EXPORT EncodeImage(const String& mime_type,
                                   const double& quality,
                                   Vector<unsigned char>* encoded_image) const;
  const unsigned char* Pixels() const { return data_; }
  const IntSize& size() const { return size_; }
  int Height() const { return size_.Height(); }
  int Width() const { return size_.Width(); }

  const unsigned char* data_;
  const IntSize size_;
};

}  // namespace blink

#endif  // ImageBuffer_h
