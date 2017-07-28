// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/MailboxTextureHolder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
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
    std::unique_ptr<gpu::Mailbox> mailbox,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    std::unique_ptr<gpu::SyncToken> sync_token) {
  if (context_provider) {
    // Avoid leaking mailboxes in cases where the texture gets recycled by skia.
    context_provider->ContextProvider()
        ->ContextGL()
        ->ProduceTextureDirectCHROMIUM(0, GL_TEXTURE_2D, mailbox->name);
  }
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
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper,
    IntSize mailbox_size)
    : TextureHolder(std::move(context_provider_wrapper)),
      mailbox_(mailbox),
      sync_token_(sync_token),
      texture_id_(texture_id_to_delete_after_mailbox_consumed),
      size_(mailbox_size),
      is_converted_from_skia_texture_(false),
      thread_id_(0) {
  InitCommon();
}

MailboxTextureHolder::MailboxTextureHolder(
    std::unique_ptr<TextureHolder> texture_holder)
    : TextureHolder(texture_holder->ContextProviderWrapper()),
      texture_id_(0),
      is_converted_from_skia_texture_(true),
      thread_id_(0) {
  DCHECK(texture_holder->IsSkiaTextureHolder());
  sk_sp<SkImage> image = texture_holder->GetSkImage();
  DCHECK(image);
  size_ = IntSize(image->width(), image->height());

  if (!ContextProvider())
    return;

  gpu::gles2::GLES2Interface* gl = ContextProvider()->ContextGL();
  GrContext* gr = ContextProvider()->GetGrContext();
  DCHECK(gl);  // IsValid() check above should guarantee this
  DCHECK(gr);
  GLuint texture_id_ =
      skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
          ->fID;
  gl->BindTexture(GL_TEXTURE_2D, texture_id_);

  gl->GenMailboxCHROMIUM(mailbox_.name);
  gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox_.name);
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());

  gl->BindTexture(GL_TEXTURE_2D, 0);
  // We changed bound textures in this function, so reset the GrContext.
  gr->resetContext(kTextureBinding_GrGLBackendState);
  InitCommon();
}

void MailboxTextureHolder::InitCommon() {
  WebThread* thread = Platform::Current()->CurrentThread();
  thread_id_ = thread->ThreadId();
  texture_thread_task_runner_ = thread->GetWebTaskRunner();
}

bool MailboxTextureHolder::IsValid() const {
  if (thread_id_ != Platform::Current()->CurrentThread()->ThreadId()) {
    // If context is is from another thread, validity cannot be verified.
    // Just assume valid. Potential problem will be detected later.
    return true;
  }
  return !!ContextProviderWrapper();
}

MailboxTextureHolder::~MailboxTextureHolder() {
  std::unique_ptr<gpu::SyncToken> passed_sync_token(
      new gpu::SyncToken(sync_token_));
  std::unique_ptr<gpu::Mailbox> passed_mailbox(new gpu::Mailbox(mailbox_));

  if (texture_thread_task_runner_ &&
      thread_id_ != Platform::Current()->CurrentThread()->ThreadId()) {
    texture_thread_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&ReleaseTexture, is_converted_from_skia_texture_,
                        texture_id_, WTF::Passed(std::move(passed_mailbox)),
                        WTF::Passed(ContextProviderWrapper()),
                        WTF::Passed(std::move(passed_sync_token))));
  } else {
    ReleaseTexture(is_converted_from_skia_texture_, texture_id_,
                   std::move(passed_mailbox), ContextProviderWrapper(),
                   std::move(passed_sync_token));
  }

  texture_id_ = 0u;  // invalidate the texture.
  texture_thread_task_runner_ = nullptr;
}

}  // namespace blink
