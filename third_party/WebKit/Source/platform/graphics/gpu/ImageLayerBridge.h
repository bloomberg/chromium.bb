// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageLayerBridge_h
#define ImageLayerBridge_h

#include "cc/layers/texture_layer_client.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Heap.h"

namespace cc {
class SharedBitmap;
}

namespace blink {

class WebLayer;
class WebExternalTextureLayer;

class PLATFORM_EXPORT ImageLayerBridge
    : public GarbageCollectedFinalized<ImageLayerBridge>,
      NON_EXPORTED_BASE(public cc::TextureLayerClient) {
  WTF_MAKE_NONCOPYABLE(ImageLayerBridge);

 public:
  ImageLayerBridge(OpacityMode);
  ~ImageLayerBridge();

  void SetImage(PassRefPtr<StaticBitmapImage>);
  void Dispose();

  // cc::TextureLayerClient implementation.
  bool PrepareTextureMailbox(cc::TextureMailbox* out_mailbox,
                             std::unique_ptr<cc::SingleReleaseCallback>*
                                 out_release_callback) override;

  void MailboxReleasedGpu(RefPtr<StaticBitmapImage>,
                          const gpu::SyncToken&,
                          bool lost_resource);

  void MailboxReleasedSoftware(std::unique_ptr<cc::SharedBitmap>,
                               const IntSize&,
                               const gpu::SyncToken&,
                               bool lost_resource);

  RefPtr<StaticBitmapImage> GetImage() { return image_; }

  WebLayer* PlatformLayer() const;

  void SetFilterQuality(SkFilterQuality filter_quality) {
    filter_quality_ = filter_quality;
  }

  bool IsAccelerated() { return image_->IsTextureBacked(); }

  DEFINE_INLINE_TRACE() {}

 private:
  std::unique_ptr<cc::SharedBitmap> CreateOrRecycleBitmap();

  RefPtr<StaticBitmapImage> image_;
  std::unique_ptr<WebExternalTextureLayer> layer_;
  SkFilterQuality filter_quality_ = kLow_SkFilterQuality;

  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this ImageLayerBridge.
  struct RecycledBitmap {
    std::unique_ptr<cc::SharedBitmap> bitmap;
    IntSize size;
  };
  Vector<RecycledBitmap> recycled_bitmaps_;

  bool disposed_ = false;
  bool has_presented_since_last_set_image_ = false;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif
