// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
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

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::createFromSharedContextImage(
    sk_sp<SkImage> image) {
  return adoptRef(new AcceleratedStaticBitmapImage(std::move(image)));
}

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::createFromWebGLContextImage(
    sk_sp<SkImage> image,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& syncToken) {
  return adoptRef(
      new AcceleratedStaticBitmapImage(std::move(image), mailbox, syncToken));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(sk_sp<SkImage> image)
    : StaticBitmapImage(std::move(image)),
      m_sharedContextId(SharedGpuContext::contextId()) {
  m_threadChecker.DetachFromThread();
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& syncToken)
    : StaticBitmapImage(std::move(image)),
      m_sharedContextId(SharedGpuContext::kNoSharedContext),
      m_hasMailbox(true),
      m_mailbox(mailbox),
      m_syncToken(syncToken) {
  m_threadChecker.DetachFromThread();

  // Note: In this case, m_image is not usable directly because it is not in the
  // shared context. It is just used to hold a reference to the texture object
  // in the origin context until the mailbox can be consumed.
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  // Avoid leaking mailboxes in cases where the texture gets recycled by skia.
  if (m_hasMailbox && SharedGpuContext::isValid())
    SharedGpuContext::gl()->ProduceTextureDirectCHROMIUM(0, GL_TEXTURE_2D,
                                                         m_mailbox.name);
  releaseImageThreadSafe();
}

void AcceleratedStaticBitmapImage::copyToTexture(
    WebGraphicsContext3DProvider* destProvider,
    GLuint destTextureId,
    GLenum internalFormat,
    GLenum destType,
    bool flipY) {
  checkThread();
  if (!isValid())
    return;
  // |destProvider| may not be the same context as the one used for |m_image|,
  // so we use a mailbox to generate a texture id for |destProvider| to access.
  ensureMailbox();

  // Get a texture id that |destProvider| knows about and copy from it.
  gpu::gles2::GLES2Interface* destGL = destProvider->contextGL();
  destGL->WaitSyncTokenCHROMIUM(m_syncToken.GetData());
  GLuint sourceTextureId =
      destGL->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
  destGL->CopyTextureCHROMIUM(sourceTextureId, destTextureId, internalFormat,
                              destType, flipY, false, false);
  // This drops the |destGL| context's reference on our |m_mailbox|, but it's
  // still held alive by our SkImage.
  destGL->DeleteTextures(1, &sourceTextureId);
}

sk_sp<SkImage> AcceleratedStaticBitmapImage::imageForCurrentFrame() {
  checkThread();
  if (!isValid())
    return nullptr;
  createImageFromMailboxIfNeeded();
  return m_image;
}

void AcceleratedStaticBitmapImage::draw(
    SkCanvas* canvas,
    const SkPaint& paint,
    const FloatRect& dstRect,
    const FloatRect& srcRect,
    RespectImageOrientationEnum respectImageOrientation,
    ImageClampingMode imageClampingMode) {
  checkThread();
  if (!isValid())
    return;
  createImageFromMailboxIfNeeded();
  StaticBitmapImage::draw(canvas, paint, dstRect, srcRect,
                          respectImageOrientation, imageClampingMode);
}

bool AcceleratedStaticBitmapImage::isValid() {
  if (!m_image)
    return false;
  if (!SharedGpuContext::isValid())
    return false;  // Gpu context was lost
  if (imageBelongsToSharedContext() &&
      m_sharedContextId != SharedGpuContext::contextId()) {
    // Gpu context was lost and restored since the resource was created.
    return false;
  }
  return true;
}

bool AcceleratedStaticBitmapImage::imageBelongsToSharedContext() {
  return m_sharedContextId != SharedGpuContext::kNoSharedContext;
}

void AcceleratedStaticBitmapImage::createImageFromMailboxIfNeeded() {
  if (imageBelongsToSharedContext())
    return;
  DCHECK(m_hasMailbox);
  gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
  GrContext* sharedGrContext = SharedGpuContext::gr();
  DCHECK(sharedGL &&
         sharedGrContext);  // context isValid already checked in callers

  sharedGL->WaitSyncTokenCHROMIUM(m_syncToken.GetData());
  GLuint sharedContextTextureId =
      sharedGL->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, m_mailbox.name);
  GrGLTextureInfo textureInfo;
  textureInfo.fTarget = GL_TEXTURE_2D;
  textureInfo.fID = sharedContextTextureId;
  GrBackendTextureDesc backendTexture;
  backendTexture.fOrigin = kBottomLeft_GrSurfaceOrigin;
  backendTexture.fWidth = size().width();
  backendTexture.fHeight = size().height();
  backendTexture.fConfig = kSkia8888_GrPixelConfig;
  backendTexture.fTextureHandle =
      skia::GrGLTextureInfoToGrBackendObject(textureInfo);

  sk_sp<SkImage> newImage =
      SkImage::MakeFromAdoptedTexture(sharedGrContext, backendTexture);
  releaseImageThreadSafe();
  m_image = newImage;

  m_sharedContextId = SharedGpuContext::contextId();
}

void AcceleratedStaticBitmapImage::ensureMailbox() {
  if (m_hasMailbox)
    return;

  DCHECK(m_image);

  gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
  GrContext* sharedGrContext = SharedGpuContext::gr();
  if (!sharedGrContext) {
    // Can happen if the context is lost. The SkImage won't be any good now
    // anyway.
    return;
  }
  GLuint imageTextureId =
      skia::GrBackendObjectToGrGLTextureInfo(m_image->getTextureHandle(true))
          ->fID;
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

void AcceleratedStaticBitmapImage::transfer() {
  checkThread();
  ensureMailbox();
  m_sharedContextId = SharedGpuContext::kNoSharedContext;
  // If |m_imageThread| is set, it means that the image has been consumed on the
  // current thread, which may happen when we have chained transfers. When that
  // is the case, we must not reset |m_imageThread|, so we ensure that
  // releaseImage() is called on the right thread.
  if (!m_imageThread)
    m_imageThread = Platform::current()->currentThread();
  m_detachThreadAtNextCheck = true;
}

void AcceleratedStaticBitmapImage::checkThread() {
  if (m_detachThreadAtNextCheck) {
    m_threadChecker.DetachFromThread();
    m_detachThreadAtNextCheck = false;
  }
  CHECK(m_threadChecker.CalledOnValidThread());
}

void releaseImage(sk_sp<SkImage>&& image,
                  std::unique_ptr<gpu::SyncToken>&& syncToken) {
  if (SharedGpuContext::isValid() && syncToken->HasData())
    SharedGpuContext::gl()->WaitSyncTokenCHROMIUM(syncToken->GetData());
  image.reset();
}

void AcceleratedStaticBitmapImage::releaseImageThreadSafe() {
  // If m_image belongs to a GrContext that is on another thread, it
  // must be released on that thread.
  if (m_imageThread && m_image &&
      m_imageThread != Platform::current()->currentThread() &&
      SharedGpuContext::isValid()) {
    gpu::gles2::GLES2Interface* sharedGL = SharedGpuContext::gl();
    std::unique_ptr<gpu::SyncToken> releaseSyncToken(new gpu::SyncToken);
    const GLuint64 fenceSync = sharedGL->InsertFenceSyncCHROMIUM();
    sharedGL->Flush();
    sharedGL->GenSyncTokenCHROMIUM(fenceSync, releaseSyncToken->GetData());
    m_imageThread->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&releaseImage, passed(std::move(m_image)),
                        passed(std::move(releaseSyncToken))));
  }
  m_image = nullptr;
  m_imageThread = nullptr;
}

}  // namespace blink
