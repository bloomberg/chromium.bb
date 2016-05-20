// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    if (!image)
        return nullptr;
    return adoptRef(new StaticBitmapImage(image));
}

PassRefPtr<StaticBitmapImage> StaticBitmapImage::create(WebExternalTextureMailbox& mailbox)
{
    return adoptRef(new StaticBitmapImage(mailbox));
}

StaticBitmapImage::StaticBitmapImage(PassRefPtr<SkImage> image) : m_image(image)
{
    ASSERT(m_image);
}

StaticBitmapImage::StaticBitmapImage(WebExternalTextureMailbox& mailbox) : m_mailbox(mailbox)
{
}

StaticBitmapImage::~StaticBitmapImage() { }

IntSize StaticBitmapImage::size() const
{
    return IntSize(m_image->width(), m_image->height());
}

bool StaticBitmapImage::currentFrameKnownToBeOpaque(MetadataMode)
{
    return m_image->isOpaque();
}

void StaticBitmapImage::draw(SkCanvas* canvas, const SkPaint& paint, const FloatRect& dstRect,
    const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode clampMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.intersect(SkRect::Make(m_image->bounds()));

    if (dstRect.isEmpty() || adjustedSrcRect.isEmpty())
        return; // Nothing to draw.

    canvas->drawImageRect(m_image.get(), adjustedSrcRect, dstRect, &paint,
        WebCoreClampingModeToSkiaRectConstraint(clampMode));

    if (ImageObserver* observer = getImageObserver())
        observer->didDraw(this);
}

PassRefPtr<SkImage> StaticBitmapImage::imageForCurrentFrame()
{
    if (m_image)
        return m_image;
    DCHECK(isMainThread());
    // In the place when we consume an ImageBitmap that is gpu texture backed,
    // create a new SkImage from that texture.
    // TODO(xidachen): make this work on a worker thread.
    OwnPtr<WebGraphicsContext3DProvider> provider = adoptPtr(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!provider)
        return nullptr;
    GrContext* grContext = provider->grContext();
    if (!grContext)
        return nullptr;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return nullptr;
    gl->WaitSyncTokenCHROMIUM(m_mailbox.syncToken);
    GLuint textureId = gl->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    GrGLTextureInfo textureInfo;
    textureInfo.fTarget = GL_TEXTURE_2D;
    textureInfo.fID = textureId;
    GrBackendTextureDesc backendTexture;
    backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
    backendTexture.fWidth = m_mailbox.textureSize.width;
    backendTexture.fHeight = m_mailbox.textureSize.height;
    backendTexture.fConfig = kSkia8888_GrPixelConfig;
    backendTexture.fTextureHandle = skia::GrGLTextureInfoToGrBackendObject(textureInfo);
    sk_sp<SkImage> skImage = SkImage::MakeFromAdoptedTexture(grContext, backendTexture);
    m_image = fromSkSp(skImage);
    return m_image;
}

} // namespace blink
