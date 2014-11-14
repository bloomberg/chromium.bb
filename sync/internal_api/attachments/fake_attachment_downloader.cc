// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "sync/internal_api/public/attachments/attachment_util.h"

namespace syncer {

FakeAttachmentDownloader::FakeAttachmentDownloader() {
}

FakeAttachmentDownloader::~FakeAttachmentDownloader() {
  DCHECK(CalledOnValidThread());
}

void FakeAttachmentDownloader::DownloadAttachment(
    const AttachmentId& attachment_id,
    const DownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  // This is happy fake downloader, it always successfully downloads empty
  // attachment.
  scoped_refptr<base::RefCountedMemory> data(new base::RefCountedBytes());
  scoped_ptr<Attachment> attachment;
  const uint32_t crc32c = ComputeCrc32c(data);
  attachment.reset(
      new Attachment(Attachment::CreateFromParts(attachment_id, data, crc32c)));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, DOWNLOAD_SUCCESS, base::Passed(&attachment)));
}

}  // namespace syncer
