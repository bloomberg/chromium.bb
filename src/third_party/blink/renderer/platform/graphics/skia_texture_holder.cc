// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace {
bool IsSkImageOriginTopLeft(sk_sp<SkImage> image) {
  GrSurfaceOrigin origin;
  image->getBackendTexture(false, &origin);
  return origin == kTopLeft_GrSurfaceOrigin;
}
}  // namespace

namespace blink {

SkiaTextureHolder::SkiaTextureHolder(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper)
    : TextureHolder(std::move(context_provider_wrapper),
                    IsSkImageOriginTopLeft(image)),
      image_(std::move(image)) {}

SkiaTextureHolder::SkiaTextureHolder(
    std::unique_ptr<TextureHolder> texture_holder)
    : TextureHolder(SharedGpuContext::ContextProviderWrapper(),
                    texture_holder->IsOriginTopLeft()) {
  DCHECK(texture_holder->IsMailboxTextureHolder());
  const gpu::Mailbox mailbox = texture_holder->GetMailbox();
  const gpu::SyncToken sync_token = texture_holder->GetSyncToken();
  const IntSize mailbox_size = texture_holder->Size();

  if (!ContextProvider())
    return;

  gpu::gles2::GLES2Interface* shared_gl = ContextProvider()->ContextGL();
  GrContext* shared_gr_context = ContextProvider()->GetGrContext();
  DCHECK(shared_gl &&
         shared_gr_context);  // context isValid already checked in callers

  shared_gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint shared_context_texture_id = 0u;
  if (mailbox.IsSharedImage()) {
    shared_context_texture_id =
        shared_gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
    shared_gl->BeginSharedImageAccessDirectCHROMIUM(
        shared_context_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    shared_image_texture_id_ = shared_context_texture_id;
  } else {
    shared_context_texture_id =
        shared_gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);
    shared_image_texture_id_ = 0u;
  }

  GrGLTextureInfo texture_info;
  texture_info.fTarget = GL_TEXTURE_2D;
  texture_info.fID = shared_context_texture_id;
  if (kN32_SkColorType == kRGBA_8888_SkColorType) {
    texture_info.fFormat = GL_RGBA8_OES;
  } else {
    DCHECK(kN32_SkColorType == kBGRA_8888_SkColorType);
    texture_info.fFormat = GL_BGRA8_EXT;
  }
  GrBackendTexture backend_texture(mailbox_size.Width(), mailbox_size.Height(),
                                   GrMipMapped::kNo, texture_info);

  GrSurfaceOrigin origin = IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin;

  image_ = SkImage::MakeFromAdoptedTexture(shared_gr_context, backend_texture,
                                           origin, kN32_SkColorType);
}

SkiaTextureHolder::~SkiaTextureHolder() {
  // Object must be destroyed on the same thread where it was created.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (shared_image_texture_id_ && ContextProvider()) {
    ContextProvider()->ContextGL()->EndSharedImageAccessDirectCHROMIUM(
        shared_image_texture_id_);
  }
}

bool SkiaTextureHolder::IsValid() const {
  return !!ContextProviderWrapper();
}

}  // namespace blink
