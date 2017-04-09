// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/MailboxTextureHolder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/graphics/SkiaTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

namespace {

void ReleaseTexture(
    bool is_converted_from_skia_texture,
    unsigned texture_id,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    std::unique_ptr<gpu::SyncToken> sync_token) {
  if (!is_converted_from_skia_texture && texture_id && context_provider) {
    context_provider->ContextProvider()->ContextGL()->WaitSyncTokenCHROMIUM(
        sync_token->GetData());
    context_provider->ContextProvider()->ContextGL()->DeleteTextures(
        1, &texture_id);
  }
}

}  // namespace

MailboxTextureHolder::MailboxTextureHolder(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id_to_delete_after_mailbox_consumed,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    IntSize mailbox_size)
    : mailbox_(mailbox),
      sync_token_(sync_token),
      texture_id_(texture_id_to_delete_after_mailbox_consumed),
      context_provider_(context_provider),
      size_(mailbox_size),
      is_converted_from_skia_texture_(false) {}

MailboxTextureHolder::MailboxTextureHolder(
    std::unique_ptr<TextureHolder> texture_holder) {
  DCHECK(texture_holder->IsSkiaTextureHolder());
  sk_sp<SkImage> image = texture_holder->GetSkImage();
  DCHECK(image);

  gpu::gles2::GLES2Interface* shared_gl = SharedGpuContext::Gl();
  GrContext* shared_gr_context = SharedGpuContext::Gr();
  if (!shared_gr_context) {
    // Can happen if the context is lost. The SkImage won't be any good now
    // anyway.
    return;
  }
  GLuint image_texture_id =
      skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
          ->fID;
  shared_gl->BindTexture(GL_TEXTURE_2D, image_texture_id);

  shared_gl->GenMailboxCHROMIUM(mailbox_.name);
  shared_gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox_.name);
  const GLuint64 fence_sync = shared_gl->InsertFenceSyncCHROMIUM();
  shared_gl->Flush();
  shared_gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());

  shared_gl->BindTexture(GL_TEXTURE_2D, 0);
  // We changed bound textures in this function, so reset the GrContext.
  shared_gr_context->resetContext(kTextureBinding_GrGLBackendState);
  size_ = IntSize(image->width(), image->height());
  texture_id_ = image_texture_id;
  is_converted_from_skia_texture_ = true;
}

MailboxTextureHolder::~MailboxTextureHolder() {
  // Avoid leaking mailboxes in cases where the texture gets recycled by skia.
  if (SharedGpuContext::IsValid()) {
    SharedGpuContext::Gl()->ProduceTextureDirectCHROMIUM(0, GL_TEXTURE_2D,
                                                         mailbox_.name);
  }
  ReleaseTextureThreadSafe();
}

void MailboxTextureHolder::ReleaseTextureThreadSafe() {
  // If this member is still null, it means we are still at the thread where
  // the m_texture was created.
  std::unique_ptr<gpu::SyncToken> passed_sync_token(
      new gpu::SyncToken(sync_token_));
  if (!WasTransferred()) {
    ReleaseTexture(is_converted_from_skia_texture_, texture_id_,
                   context_provider_, std::move(passed_sync_token));
  } else if (WasTransferred() && TextureThreadTaskRunner()) {
    TextureThreadTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&ReleaseTexture, is_converted_from_skia_texture_,
                        texture_id_, WTF::Passed(std::move(context_provider_)),
                        WTF::Passed(std::move(passed_sync_token))));
  }
  texture_id_ = 0u;  // invalidate the texture.
  SetWasTransferred(false);
  SetTextureThreadTaskRunner(nullptr);
}

unsigned MailboxTextureHolder::SharedContextId() {
  return SharedGpuContext::kNoSharedContext;
}

}  // namespace blink
