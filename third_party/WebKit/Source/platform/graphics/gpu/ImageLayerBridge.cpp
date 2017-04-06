// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/ImageLayerBridge.h"

#include "cc/resources/shared_bitmap.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

ImageLayerBridge::ImageLayerBridge(OpacityMode opacityMode)
    : m_weakPtrFactory(this), m_opacityMode(opacityMode) {
  m_layer =
      Platform::current()->compositorSupport()->createExternalTextureLayer(
          this);
  GraphicsLayer::registerContentsLayer(m_layer->layer());
  m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
  if (m_opacityMode == Opaque) {
    m_layer->setOpaque(true);
    m_layer->setBlendBackgroundColor(false);
  }
}

ImageLayerBridge::~ImageLayerBridge() {
  if (!m_disposed)
    dispose();
}

void ImageLayerBridge::setImage(PassRefPtr<StaticBitmapImage> image) {
  m_image = std::move(image);
  if (m_image) {
    if (m_opacityMode == NonOpaque) {
      m_layer->setOpaque(m_image->currentFrameKnownToBeOpaque());
      m_layer->setBlendBackgroundColor(!m_image->currentFrameKnownToBeOpaque());
    }
  }
  if (!m_hasPresentedSinceLastSetImage && m_image &&
      m_image->isTextureBacked()) {
    // If the layer bridge is not presenting, the GrContext may not be getting
    // flushed regularly.  The flush is normally triggered inside the
    // m_image->ensureMailbox() call of
    // ImageLayerBridge::PrepareTextureMailbox. To prevent a potential memory
    // leak we must flush the GrContext here.
    m_image->imageForCurrentFrame()->getTextureHandle(
        true);  // GrContext flush.
  }
  m_hasPresentedSinceLastSetImage = false;
}

void ImageLayerBridge::dispose() {
  if (m_layer) {
    GraphicsLayer::unregisterContentsLayer(m_layer->layer());
    m_layer->clearTexture();
    m_layer.reset();
  }
  m_image = nullptr;
  m_disposed = true;
}

bool ImageLayerBridge::PrepareTextureMailbox(
    cc::TextureMailbox* outMailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback) {
  if (m_disposed)
    return false;

  if (!m_image)
    return false;

  if (m_hasPresentedSinceLastSetImage)
    return false;

  m_hasPresentedSinceLastSetImage = true;

  if (m_image->isTextureBacked()) {
    m_image->ensureMailbox();
    *outMailbox = cc::TextureMailbox(m_image->mailbox(), m_image->syncToken(),
                                     GL_TEXTURE_2D);
    auto func = WTF::bind(&ImageLayerBridge::mailboxReleasedGpu,
                          m_weakPtrFactory.createWeakPtr(), m_image);
    *outReleaseCallback = cc::SingleReleaseCallback::Create(
        convertToBaseCallback(std::move(func)));
  } else {
    std::unique_ptr<cc::SharedBitmap> bitmap = createOrRecycleBitmap();
    if (!bitmap)
      return false;

    sk_sp<SkImage> skImage = m_image->imageForCurrentFrame();
    if (!skImage)
      return false;

    SkImageInfo dstInfo = SkImageInfo::MakeN32Premul(m_image->width(), 1);
    size_t rowBytes = m_image->width() * 4;

    // loop to flip Y
    for (int row = 0; row < m_image->height(); row++) {
      if (!skImage->readPixels(
              dstInfo,
              bitmap->pixels() + rowBytes * (m_image->height() - 1 - row),
              rowBytes, 0, row))
        return false;
    }

    *outMailbox = cc::TextureMailbox(
        bitmap.get(), gfx::Size(m_image->width(), m_image->height()));
    auto func = WTF::bind(&ImageLayerBridge::mailboxReleasedSoftware,
                          m_weakPtrFactory.createWeakPtr(),
                          base::Passed(&bitmap), m_image->size());
    *outReleaseCallback = cc::SingleReleaseCallback::Create(
        convertToBaseCallback(std::move(func)));
  }

  outMailbox->set_nearest_neighbor(m_filterQuality == kNone_SkFilterQuality);
  // TODO(junov): Figure out how to get the color space info.
  // outMailbox->set_color_space();

  return true;
}

std::unique_ptr<cc::SharedBitmap> ImageLayerBridge::createOrRecycleBitmap() {
  auto it = std::remove_if(m_recycledBitmaps.begin(), m_recycledBitmaps.end(),
                           [this](const RecycledBitmap& bitmap) {
                             return bitmap.size != m_image->size();
                           });
  m_recycledBitmaps.shrink(it - m_recycledBitmaps.begin());

  if (!m_recycledBitmaps.isEmpty()) {
    RecycledBitmap recycled = std::move(m_recycledBitmaps.back());
    m_recycledBitmaps.pop_back();
    DCHECK(recycled.size == m_image->size());
    return std::move(recycled.bitmap);
  }
  return Platform::current()->allocateSharedBitmap(m_image->size());
}

void ImageLayerBridge::mailboxReleasedGpu(RefPtr<StaticBitmapImage> image,
                                          const gpu::SyncToken& token,
                                          bool lostResource) {
  if (image) {
    DCHECK(image->isTextureBacked());
    if (token.HasData()) {
      if (image->hasMailbox()) {
        image->updateSyncToken(token);
      } else {
        // Wait on sync token now because SkiaTextureHolder does not know
        // about sync tokens.
        gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
        if (sharedGL)
          sharedGL->WaitSyncTokenCHROMIUM(token.GetConstData());
      }
    }
  }
  // let 'image' go out of scope to finalize the release. The
  // destructor will wait on sync token before deleting resource.
}

void ImageLayerBridge::mailboxReleasedSoftware(
    std::unique_ptr<cc::SharedBitmap> bitmap,
    const IntSize& size,
    const gpu::SyncToken& syncToken,
    bool lostResource) {
  DCHECK(!syncToken.HasData());  // No sync tokens for software resources.
  if (!m_disposed && !lostResource) {
    RecycledBitmap recycled = {std::move(bitmap), size};
    m_recycledBitmaps.push_back(std::move(recycled));
  }
}

WebLayer* ImageLayerBridge::platformLayer() const {
  return m_layer->layer();
}

}  // namespace blink
