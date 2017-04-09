// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureHolder_h
#define TextureHolder_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT TextureHolder {
 public:
  virtual ~TextureHolder() {}

  // Methods overrided by all sub-classes
  virtual bool IsSkiaTextureHolder() = 0;
  virtual bool IsMailboxTextureHolder() = 0;
  virtual unsigned SharedContextId() = 0;
  virtual IntSize size() const = 0;
  virtual bool CurrentFrameKnownToBeOpaque(Image::MetadataMode) = 0;

  // Methods overrided by MailboxTextureHolder
  virtual gpu::Mailbox GetMailbox() {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual gpu::SyncToken GetSyncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  virtual void UpdateSyncToken(gpu::SyncToken) { NOTREACHED(); }

  // Methods overrided by SkiaTextureHolder
  virtual sk_sp<SkImage> GetSkImage() {
    NOTREACHED();
    return nullptr;
  }
  virtual void SetSharedContextId(unsigned) { NOTREACHED(); }
  virtual void SetImageThread(WebThread*) { NOTREACHED(); }
  virtual void SetImageThreadTaskRunner(RefPtr<WebTaskRunner>) { NOTREACHED(); }

  // Methods that have exactly the same impelmentation for all sub-classes
  bool WasTransferred() { return was_transferred_; }
  RefPtr<WebTaskRunner> TextureThreadTaskRunner() {
    return texture_thread_task_runner_;
  }
  void SetWasTransferred(bool flag) { was_transferred_ = flag; }
  void SetTextureThreadTaskRunner(RefPtr<WebTaskRunner> task_runner) {
    texture_thread_task_runner_ = std::move(task_runner);
  }

 private:
  // Wether the AcceleratedStaticBitmapImage that holds the |m_texture| was
  // transferred to another thread or not. Set to false when the
  // AcceleratedStaticBitmapImage remain on the same thread as it was craeted.
  bool was_transferred_ = false;
  // Keep a clone of the WebTaskRunner. This is to handle the case where the
  // AcceleratedStaticBitmapImage was created on one thread and transferred to
  // another thread, and the original thread gone out of scope, and that we need
  // to clear the resouces associated with that AcceleratedStaticBitmapImage on
  // the original thread.
  RefPtr<WebTaskRunner> texture_thread_task_runner_;
};

}  // namespace blink

#endif  // TextureHolder_h
