// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_downloader_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace syncer {

AttachmentDownloaderImpl::AttachmentDownloaderImpl() {
}

AttachmentDownloaderImpl::~AttachmentDownloaderImpl() {
  DCHECK(CalledOnValidThread());
}

void AttachmentDownloaderImpl::DownloadAttachment(
    const AttachmentId& attachment_id,
    const DownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  // No real implementation yet. Fail every request with
  // DOWNLOAD_UNSPECIFIED_ERROR.
  scoped_ptr<Attachment> attachment;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          callback, DOWNLOAD_UNSPECIFIED_ERROR, base::Passed(&attachment)));
}

}  // namespace syncer
