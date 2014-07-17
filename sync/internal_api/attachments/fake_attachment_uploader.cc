// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

FakeAttachmentUploader::FakeAttachmentUploader() {
  DCHECK(CalledOnValidThread());
}

FakeAttachmentUploader::~FakeAttachmentUploader() {
  DCHECK(CalledOnValidThread());
}

void FakeAttachmentUploader::UploadAttachment(const Attachment& attachment,
                                              const UploadCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!attachment.GetId().GetProto().unique_id().empty());

  UploadResult result = UPLOAD_SUCCESS;
  AttachmentId id = attachment.GetId();
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, result, id));
}

}  // namespace syncer
