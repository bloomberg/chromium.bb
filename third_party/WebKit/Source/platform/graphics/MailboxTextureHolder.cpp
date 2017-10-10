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

  if (!ContextProviderWrapper())
    return;

  ContextProviderWrapper()->Utils()->GetMailboxForSkImage(mailbox_, image);

  InitCommon();
}

void MailboxTextureHolder::Sync(MailboxSyncMode mode) {
  if (thread_id_ != Platform::Current()->CurrentThread()->ThreadId()) {
    // Was originally created on another thread. Should already have a sync
    // token from the original source context, already verified if needed.
    DCHECK(sync_token_.HasData());
    DCHECK(mode != kVerifiedSyncToken || sync_token_.verified_flush());
    return;
  }

  if (!ContextProviderWrapper() || IsAbandoned())
    return;

  gpu::gles2::GLES2Interface* gl =
      ContextProviderWrapper()->ContextProvider()->ContextGL();

  if (mode == kOrderingBarrier) {
    if (!did_issue_ordering_barrier_) {
      gl->OrderingBarrierCHROMIUM();
      did_issue_ordering_barrier_ = true;
    }
    return;
  }

  if (!sync_token_.HasData()) {
    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    if (mode == kVerifiedSyncToken) {
      gl->ShallowFlushCHROMIUM();
      gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());
    } else {
      gl->OrderingBarrierCHROMIUM();
      gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());
    }
    return;
  }

  // At this point we have a pre-existing sync token. We just need to verify
  // it if needed.  Providing a verified sync token when unverified is requested
  // is fine.
  if (mode == kVerifiedSyncToken && !sync_token_.verified_flush()) {
    int8_t* token_data = sync_token_.GetData();
    // TODO(junov): Batch this verification in the case where there are multiple
    // offscreen canvases being committed.
    gl->ShallowFlushCHROMIUM();
    gl->VerifySyncTokensCHROMIUM(&token_data, 1);
    sync_token_.SetVerifyFlush();
  }
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
  return !IsAbandoned() && !!ContextProviderWrapper();
}

MailboxTextureHolder::~MailboxTextureHolder() {
  std::unique_ptr<gpu::SyncToken> passed_sync_token(
      new gpu::SyncToken(sync_token_));
  std::unique_ptr<gpu::Mailbox> passed_mailbox(new gpu::Mailbox(mailbox_));

  if (!IsAbandoned()) {
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
  }

  texture_id_ = 0u;  // invalidate the texture.
  texture_thread_task_runner_ = nullptr;
}

}  // namespace blink
