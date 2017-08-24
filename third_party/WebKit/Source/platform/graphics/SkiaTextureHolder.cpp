// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SkiaTextureHolder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/MailboxTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

SkiaTextureHolder::SkiaTextureHolder(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper)
    : TextureHolder(std::move(context_provider_wrapper)),
      image_(std::move(image)) {}

SkiaTextureHolder::SkiaTextureHolder(
    std::unique_ptr<TextureHolder> texture_holder)
    : TextureHolder(SharedGpuContext::ContextProviderWrapper()) {
  DCHECK(texture_holder->IsMailboxTextureHolder());
  const gpu::Mailbox mailbox = texture_holder->GetMailbox();
  const gpu::SyncToken sync_token = texture_holder->GetSyncToken();
  const IntSize mailbox_size = texture_holder->Size();

  if (!ContextProvider())
    return;
  if (texture_holder->IsAbandoned()) {
    Abandon();
    return;
  }

  gpu::gles2::GLES2Interface* shared_gl = ContextProvider()->ContextGL();
  GrContext* shared_gr_context = ContextProvider()->GetGrContext();
  DCHECK(shared_gl &&
         shared_gr_context);  // context isValid already checked in callers

  shared_gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint shared_context_texture_id =
      shared_gl->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  GrGLTextureInfo texture_info;
  texture_info.fTarget = GL_TEXTURE_2D;
  texture_info.fID = shared_context_texture_id;
  GrBackendTexture backend_texture(mailbox_size.Width(), mailbox_size.Height(),
                                   kSkia8888_GrPixelConfig, texture_info);
  image_ = SkImage::MakeFromAdoptedTexture(shared_gr_context, backend_texture,
                                           kBottomLeft_GrSurfaceOrigin);
}

SkiaTextureHolder::~SkiaTextureHolder() {
  // Object must be destroyed on the same thread where it was created.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaTextureHolder::Abandon() {
  if (image_ && image_->getTexture())
    image_->getTexture()->abandon();
  TextureHolder::Abandon();
}

bool SkiaTextureHolder::IsValid() const {
  return !IsAbandoned() && !!ContextProviderWrapper();
}

}  // namespace blink
