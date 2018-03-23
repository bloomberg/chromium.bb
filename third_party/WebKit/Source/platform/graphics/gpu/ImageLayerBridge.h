// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageLayerBridge_h
#define ImageLayerBridge_h

#include "cc/layers/texture_layer_client.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Heap.h"

namespace viz {
class SharedBitmap;
}

namespace blink {

class WebLayer;
class WebExternalTextureLayer;

class PLATFORM_EXPORT ImageLayerBridge
    : public GarbageCollectedFinalized<ImageLayerBridge>,
      public cc::TextureLayerClient {
  WTF_MAKE_NONCOPYABLE(ImageLayerBridge);

 public:
  ImageLayerBridge(OpacityMode);
  ~ImageLayerBridge();

  void SetImage(scoped_refptr<StaticBitmapImage>);
  void Dispose();

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(viz::TransferableResource* out_resource,
                                   std::unique_ptr<viz::SingleReleaseCallback>*
                                       out_release_callback) override;

  void ResourceReleasedGpu(scoped_refptr<StaticBitmapImage>,
                           const gpu::SyncToken&,
                           bool lost_resource);

  void ResourceReleasedSoftware(std::unique_ptr<viz::SharedBitmap>,
                                const IntSize&,
                                const gpu::SyncToken&,
                                bool lost_resource);

  scoped_refptr<StaticBitmapImage> GetImage() { return image_; }

  WebLayer* PlatformLayer() const;

  void SetFilterQuality(SkFilterQuality filter_quality) {
    filter_quality_ = filter_quality;
  }
  void SetUV(const FloatPoint left_top, const FloatPoint right_bottom);

  bool IsAccelerated() { return image_->IsTextureBacked(); }

  void Trace(blink::Visitor* visitor) {}

 private:
  std::unique_ptr<viz::SharedBitmap> CreateOrRecycleBitmap(const IntSize& size);

  scoped_refptr<StaticBitmapImage> image_;
  std::unique_ptr<WebExternalTextureLayer> layer_;
  SkFilterQuality filter_quality_ = kLow_SkFilterQuality;

  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this ImageLayerBridge.
  struct RecycledBitmap {
    std::unique_ptr<viz::SharedBitmap> bitmap;
    IntSize size;
  };
  Vector<RecycledBitmap> recycled_bitmaps_;

  bool disposed_ = false;
  bool has_presented_since_last_set_image_ = false;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif
