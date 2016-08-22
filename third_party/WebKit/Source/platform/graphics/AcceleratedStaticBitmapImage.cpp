// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "wtf/PtrUtil.h"

#include <memory>
#include <utility>

namespace blink {

PassRefPtr<AcceleratedStaticBitmapImage> AcceleratedStaticBitmapImage::create(PassRefPtr<SkImage> image)
{
    return adoptRef(new AcceleratedStaticBitmapImage(image));
}

PassRefPtr<AcceleratedStaticBitmapImage> AcceleratedStaticBitmapImage::create(PassRefPtr<SkImage> image, sk_sp<GrContext> grContext, const gpu::Mailbox& mailbox, const gpu::SyncToken& syncToken)
{
    return adoptRef(new AcceleratedStaticBitmapImage(image, std::move(grContext), mailbox, syncToken));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(PassRefPtr<SkImage> image)
    : StaticBitmapImage(std::move(image))
    , m_imageIsForSharedMainThreadContext(true)
{
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(PassRefPtr<SkImage> image, sk_sp<GrContext> grContext, const gpu::Mailbox& mailbox, const gpu::SyncToken& syncToken)
    : StaticBitmapImage(std::move(image))
    , m_imageIsForSharedMainThreadContext(false) // TODO(danakj): Could be true though, caller would know.
    , m_grContext(std::move(grContext))
    , m_hasMailbox(true)
    , m_mailbox(mailbox)
    , m_syncToken(syncToken)
{
    DCHECK(m_grContext);
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() = default;

void AcceleratedStaticBitmapImage::copyToTexture(WebGraphicsContext3DProvider* destProvider, GLuint destTextureId, GLenum internalFormat, GLenum destType, bool flipY)
{
    // |destProvider| may not be the same context as the one used for |m_image| so we use a mailbox to
    // generate a texture id for |destProvider| to access.
    ensureMailbox();

    // Get a texture id that |destProvider| knows about and copy from it.
    gpu::gles2::GLES2Interface* destGL = destProvider->contextGL();
    destGL->WaitSyncTokenCHROMIUM(m_syncToken.GetData());
    GLuint sourceTextureId = destGL->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    destGL->CopyTextureCHROMIUM(sourceTextureId, destTextureId, internalFormat, destType, flipY, false, false);
    // This drops the |destGL| context's reference on our |m_mailbox|, but it's still held alive by our SkImage.
    destGL->DeleteTextures(1, &sourceTextureId);
}

PassRefPtr<SkImage> AcceleratedStaticBitmapImage::imageForCurrentFrame()
{
    // This must return an SkImage that can be used with the shared main thread context. If |m_image| satisfies that, we are done.
    if (m_imageIsForSharedMainThreadContext)
        return m_image;

    // TODO(xidachen): make this work on a worker thread.
    DCHECK(isMainThread());

    // If the SkImage came from any other context than the shared main thread one, we expect to be given a mailbox at construction. We
    // use the mailbox to generate a texture id for the shared main thread context to use.
    DCHECK(m_hasMailbox);

    auto sharedMainThreadContextProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    gpu::gles2::GLES2Interface* sharedGL = sharedMainThreadContextProvider->contextGL();
    GrContext* sharedGrContext = sharedMainThreadContextProvider->grContext();
    if (!sharedGrContext)
        return nullptr; // Can happen if the context is lost, the SkImage won't be any good now anyway.

    sharedGL->WaitSyncTokenCHROMIUM(m_syncToken.GetData());
    GLuint sharedContextTextureId = sharedGL->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);

    GrGLTextureInfo textureInfo;
    textureInfo.fTarget = GL_TEXTURE_2D;
    textureInfo.fID = sharedContextTextureId;
    GrBackendTextureDesc backendTexture;
    backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
    backendTexture.fWidth = size().width();
    backendTexture.fHeight = size().height();
    backendTexture.fConfig = kSkia8888_GrPixelConfig;
    backendTexture.fTextureHandle = skia::GrGLTextureInfoToGrBackendObject(textureInfo);

    m_image = fromSkSp(SkImage::MakeFromAdoptedTexture(sharedGrContext, backendTexture));
    m_imageIsForSharedMainThreadContext = true;
    // Can drop the ref on the GrContext since m_image is now backed by a texture from the shared main thread context.
    m_grContext = nullptr;
    return m_image;
}

void AcceleratedStaticBitmapImage::ensureMailbox()
{
    if (m_hasMailbox)
        return;

    // If we weren't given a mailbox at creation, then we were given a SkImage that is assumed to be from the shared main thread context.
    DCHECK(m_imageIsForSharedMainThreadContext);
    auto sharedMainThreadContextProvider = wrapUnique(Platform::current()->createSharedOffscreenGraphicsContext3DProvider());

    gpu::gles2::GLES2Interface* sharedGL = sharedMainThreadContextProvider->contextGL();
    GrContext* sharedGrContext = sharedMainThreadContextProvider->grContext();
    if (!sharedGrContext)
        return; // Can happen if the context is lost, the SkImage won't be any good now anyway.

    GLuint imageTextureId = skia::GrBackendObjectToGrGLTextureInfo(m_image->getTextureHandle(true))->fID;
    sharedGL->BindTexture(GL_TEXTURE_2D, imageTextureId);

    sharedGL->GenMailboxCHROMIUM(m_mailbox.name);
    sharedGL->ProduceTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
    const GLuint64 fenceSync = sharedGL->InsertFenceSyncCHROMIUM();
    sharedGL->Flush();
    sharedGL->GenSyncTokenCHROMIUM(fenceSync, m_syncToken.GetData());

    sharedGL->BindTexture(GL_TEXTURE_2D, 0);
    // We changed bound textures in this function, so reset the GrContext.
    sharedGrContext->resetContext(kTextureBinding_GrGLBackendState);

    m_hasMailbox = true;
}

} // namespace blink
