// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MailboxTextureHolder_h
#define MailboxTextureHolder_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/TextureHolder.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/wtf/WeakPtr.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class PLATFORM_EXPORT MailboxTextureHolder final : public TextureHolder {
 public:
  ~MailboxTextureHolder() override;

  bool IsSkiaTextureHolder() final { return false; }
  bool IsMailboxTextureHolder() final { return true; }
  IntSize Size() const final { return size_; }
  bool CurrentFrameKnownToBeOpaque(Image::MetadataMode) final { return false; }
  bool IsValid() const final;

  gpu::Mailbox GetMailbox() final { return mailbox_; }
  gpu::SyncToken GetSyncToken() final { return sync_token_; }
  void UpdateSyncToken(gpu::SyncToken sync_token) final {
    sync_token_ = sync_token;
  }

  void Sync(MailboxSyncMode) final;
  // In WebGL's commit or transferToImageBitmap calls, it will call the
  // DrawingBuffer::transferToStaticBitmapImage function, which produces the
  // input parameters for this method.
  MailboxTextureHolder(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       unsigned texture_id_to_delete_after_mailbox_consumed,
                       WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
                       IntSize mailbox_size);
  // This function turns a texture-backed SkImage into a mailbox and a
  // syncToken.
  MailboxTextureHolder(std::unique_ptr<TextureHolder>);

 private:
  void InitCommon();

  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;
  unsigned texture_id_;
  IntSize size_;
  bool is_converted_from_skia_texture_;
  RefPtr<WebTaskRunner> texture_thread_task_runner_;
  PlatformThreadId thread_id_;
  bool did_issue_ordering_barrier_ = false;
};

}  // namespace blink

#endif  // MailboxTextureHolder_h
