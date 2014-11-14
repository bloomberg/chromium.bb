// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include "base/logging.h"
#include "sync/internal_api/public/attachments/attachment_util.h"

namespace syncer {

Attachment::~Attachment() {}

// Static.
Attachment Attachment::Create(
    const scoped_refptr<base::RefCountedMemory>& data) {
  uint32_t crc32c = ComputeCrc32c(data);
  return CreateFromParts(AttachmentId::Create(), data, crc32c);
}

// Static.
Attachment Attachment::CreateFromParts(
    const AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& data,
    uint32_t crc32c) {
  return Attachment(id, data, crc32c);
}

const AttachmentId& Attachment::GetId() const { return id_; }

const scoped_refptr<base::RefCountedMemory>& Attachment::GetData() const {
  return data_;
}

uint32_t Attachment::GetCrc32c() const { return crc32c_; }

Attachment::Attachment(const AttachmentId& id,
                       const scoped_refptr<base::RefCountedMemory>& data,
                       uint32_t crc32c)
    : id_(id), data_(data), crc32c_(crc32c) {
  DCHECK(data.get());
}

}  // namespace syncer
