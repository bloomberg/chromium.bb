// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageLayerBridge_h
#define ImageLayerBridge_h

#include "cc/layers/texture_layer_client.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Heap.h"
#include "wtf/WeakPtr.h"

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

  void setImage(PassRefPtr<StaticBitmapImage>);
  void dispose();

  // cc::TextureLayerClient implementation.
  bool PrepareTextureMailbox(
      cc::TextureMailbox* outMailbox,
      std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback) override;

  void mailboxReleasedGpu(RefPtr<StaticBitmapImage>,
                          const gpu::SyncToken&,
                          bool lostResource);

  void mailboxReleasedSoftware(std::unique_ptr<cc::SharedBitmap>,
                               const IntSize&,
                               const gpu::SyncToken&,
                               bool lostResource);

  RefPtr<StaticBitmapImage> image() { return m_image; }

  WebLayer* platformLayer() const;

  void setFilterQuality(SkFilterQuality filterQuality) {
    m_filterQuality = filterQuality;
  }

  bool isAccelerated() { return m_image->isTextureBacked(); }

  DEFINE_INLINE_TRACE() {}

 private:
  std::unique_ptr<cc::SharedBitmap> createOrRecycleBitmap();

  WeakPtrFactory<ImageLayerBridge> m_weakPtrFactory;
  RefPtr<StaticBitmapImage> m_image;
  std::unique_ptr<WebExternalTextureLayer> m_layer;
  SkFilterQuality m_filterQuality = kLow_SkFilterQuality;

  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this ImageLayerBridge.
  struct RecycledBitmap {
    std::unique_ptr<cc::SharedBitmap> bitmap;
    IntSize size;
  };
  Vector<RecycledBitmap> m_recycledBitmaps;

  bool m_disposed = false;
  bool m_hasPresentedSinceLastSetImage = false;
  OpacityMode m_opacityMode = NonOpaque;
};

}  // namespace blink

#endif
