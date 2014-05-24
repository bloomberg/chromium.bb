// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_

#include "base/threading/non_thread_safe.h"
#include "sync/api/attachments/attachment_downloader.h"

namespace syncer {

// FakeAttachmentDownloader is for tests. For every request it posts a success
// callback with empty attachment.
class SYNC_EXPORT FakeAttachmentDownloader : public AttachmentDownloader,
                                             public base::NonThreadSafe {
 public:
  FakeAttachmentDownloader();
  virtual ~FakeAttachmentDownloader();

  // AttachmentDownloader implementation.
  virtual void DownloadAttachment(const AttachmentId& attachment_id,
                                  const DownloadCallback& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAttachmentDownloader);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_
