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

namespace {

void ReleaseImage(sk_sp<SkImage>&& image,
                  std::unique_ptr<gpu::SyncToken>&& sync_token) {
  if (SharedGpuContext::IsValid() && sync_token->HasData())
    SharedGpuContext::Gl()->WaitSyncTokenCHROMIUM(sync_token->GetData());
  image.reset();
}

}  // namespace

SkiaTextureHolder::SkiaTextureHolder(sk_sp<SkImage> image)
    : image_(std::move(image)),
      shared_context_id_(SharedGpuContext::ContextId()) {}

SkiaTextureHolder::SkiaTextureHolder(
    std::unique_ptr<TextureHolder> texture_holder) {
  DCHECK(texture_holder->IsMailboxTextureHolder());
  const gpu::Mailbox mailbox = texture_holder->GetMailbox();
  const gpu::SyncToken sync_token = texture_holder->GetSyncToken();
  const IntSize mailbox_size = texture_holder->Size();

  gpu::gles2::GLES2Interface* shared_gl = SharedGpuContext::Gl();
  GrContext* shared_gr_context = SharedGpuContext::Gr();
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
  sk_sp<SkImage> new_image = SkImage::MakeFromAdoptedTexture(
      shared_gr_context, backend_texture, kBottomLeft_GrSurfaceOrigin);
  ReleaseImageThreadSafe();
  image_ = new_image;
  shared_context_id_ = SharedGpuContext::ContextId();
}

SkiaTextureHolder::~SkiaTextureHolder() {
  ReleaseImageThreadSafe();
}

unsigned SkiaTextureHolder::SharedContextId() {
  return shared_context_id_;
}

void SkiaTextureHolder::ReleaseImageThreadSafe() {
  // If m_image belongs to a GrContext that is on another thread, it
  // must be released on that thread.
  if (TextureThreadTaskRunner() && image_ && WasTransferred() &&
      SharedGpuContext::IsValid()) {
    gpu::gles2::GLES2Interface* shared_gl = SharedGpuContext::Gl();
    std::unique_ptr<gpu::SyncToken> release_sync_token(new gpu::SyncToken);
    const GLuint64 fence_sync = shared_gl->InsertFenceSyncCHROMIUM();
    shared_gl->Flush();
    shared_gl->GenSyncTokenCHROMIUM(fence_sync, release_sync_token->GetData());
    TextureThreadTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&ReleaseImage, WTF::Passed(std::move(image_)),
                        WTF::Passed(std::move(release_sync_token))));
  }
  image_ = nullptr;
  SetWasTransferred(false);
  SetTextureThreadTaskRunner(nullptr);
}

}  // namespace blink
