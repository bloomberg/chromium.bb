// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_DUMMY_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_DUMMY_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

// Dummy implementation to be used instead of MailboxManagerSync when shared
// images are enabled on webview. None of the below methods needs to do any
// operation when shared images are enabled hence making all no-op.
class GPU_GLES2_EXPORT MailboxManagerDummy : public MailboxManager {
 public:
  MailboxManagerDummy();
  ~MailboxManagerDummy() override;

  // MailboxManager implementation:
  TextureBase* ConsumeTexture(const Mailbox& mailbox) override;
  void ProduceTexture(const Mailbox& mailbox, TextureBase* texture) override {}
  bool UsesSync() override;
  void PushTextureUpdates(const SyncToken& token) override {}
  void PullTextureUpdates(const SyncToken& token) override {}
  void TextureDeleted(TextureBase* texture) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MailboxManagerDummy);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_DUMMY_H_
