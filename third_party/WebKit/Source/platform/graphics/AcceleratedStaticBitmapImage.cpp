// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PassRefPtr<AcceleratedStaticBitmapImage> AcceleratedStaticBitmapImage::create(WebExternalTextureMailbox& mailbox)
{
    return adoptRef(new AcceleratedStaticBitmapImage(mailbox));
}

PassRefPtr<AcceleratedStaticBitmapImage> AcceleratedStaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    return adoptRef(new AcceleratedStaticBitmapImage(image));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(WebExternalTextureMailbox& mailbox)
    : StaticBitmapImage()
    , m_mailbox(mailbox)
{
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(PassRefPtr<SkImage> image)
    : StaticBitmapImage(image)
{
}

IntSize AcceleratedStaticBitmapImage::size() const
{
    if (m_image)
        return IntSize(m_image->width(), m_image->height());
    return IntSize(m_mailbox.textureSize.width, m_mailbox.textureSize.height);
}

void AcceleratedStaticBitmapImage::copyToTexture(WebGraphicsContext3DProvider* provider, GLuint destinationTexture, GLenum internalFormat, GLenum destType, bool flipY)
{
    GLuint textureId = switchStorageToSkImageForWebGL(provider);
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return;
    gl->CopyTextureCHROMIUM(textureId, destinationTexture, internalFormat, destType, flipY, false, false);
    const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();
    GLbyte syncToken[24];
    gl->GenSyncTokenCHROMIUM(fenceSync, syncToken);
    // Get a new mailbox because we cannot retain a texture in the WebGL context.
    switchStorageToMailbox(provider);
}

bool AcceleratedStaticBitmapImage::switchStorageToMailbox(WebGraphicsContext3DProvider* provider)
{
    m_mailbox.textureSize = WebSize(m_image->width(), m_image->height());
    GrContext* grContext = provider->grContext();
    if (!grContext)
        return false;
    grContext->flush();
    m_mailbox.textureTarget = GL_TEXTURE_2D;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return false;
    GLuint textureID = skia::GrBackendObjectToGrGLTextureInfo(m_image->getTextureHandle(true))->fID;
    gl->BindTexture(GL_TEXTURE_2D, textureID);

    gl->GenMailboxCHROMIUM(m_mailbox.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();
    gl->GenSyncTokenCHROMIUM(fenceSync, m_mailbox.syncToken);
    m_mailbox.validSyncToken = true;
    gl->BindTexture(GL_TEXTURE_2D, 0);
    grContext->resetContext(kTextureBinding_GrGLBackendState);
    m_image = nullptr;
    return true;
}

// This function is called only in the case that m_image is texture backed.
GLuint AcceleratedStaticBitmapImage::switchStorageToSkImageForWebGL(WebGraphicsContext3DProvider* contextProvider)
{
    DCHECK(!m_image || m_image->isTextureBacked());
    GLuint textureId = 0;
    if (m_image) {
        // SkImage is texture-backed on the shared context
        if (!hasMailbox()) {
            std::unique_ptr<WebGraphicsContext3DProvider> sharedProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
            if (!switchStorageToMailbox(sharedProvider.get()))
                return 0;
            textureId = switchStorageToSkImage(contextProvider);
            return textureId;
        }
    }
    DCHECK(hasMailbox());
    textureId = switchStorageToSkImage(contextProvider);
    return textureId;
}

GLuint AcceleratedStaticBitmapImage::switchStorageToSkImage(WebGraphicsContext3DProvider* provider)
{
    if (!provider)
        return 0;
    GrContext* grContext = provider->grContext();
    if (!grContext)
        return 0;
    gpu::gles2::GLES2Interface* gl = provider->contextGL();
    if (!gl)
        return 0;
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
    return textureId;
}

PassRefPtr<SkImage> AcceleratedStaticBitmapImage::imageForCurrentFrame()
{
    if (m_image)
        return m_image;
    if (!hasMailbox())
        return nullptr;
    // Has mailbox, consume mailbox, prepare a new mailbox if contextProvider is not null (3D).
    DCHECK(isMainThread());
    // TODO(xidachen): make this work on a worker thread.
    std::unique_ptr<WebGraphicsContext3DProvider> sharedProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!switchStorageToSkImage(sharedProvider.get()))
        return nullptr;
    return m_image;
}

} // namespace blink
