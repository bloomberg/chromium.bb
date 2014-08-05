// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_

#include "base/threading/non_thread_safe.h"
#include "sync/internal_api/public/attachments/attachment_uploader.h"

namespace syncer {

// A fake implementation of AttachmentUploader.
class SYNC_EXPORT FakeAttachmentUploader : public AttachmentUploader,
                                           public base::NonThreadSafe {
 public:
  FakeAttachmentUploader();
  virtual ~FakeAttachmentUploader();

  // AttachmentUploader implementation.
  virtual void UploadAttachment(const Attachment& attachment,
                                const UploadCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAttachmentUploader);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_
