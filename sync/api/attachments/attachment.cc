// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/rand_util.h"

namespace syncer {

Attachment::~Attachment() {}

// Static.
scoped_ptr<Attachment> Attachment::Create(
    const scoped_refptr<base::RefCountedMemory>& bytes) {
  return CreateWithId(CreateId(), bytes);
}

// Static.
scoped_ptr<Attachment> Attachment::CreateWithId(
    const sync_pb::AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& bytes) {
  return scoped_ptr<Attachment>(new Attachment(id, bytes)).Pass();
}

const sync_pb::AttachmentId& Attachment::GetId() const { return id_; }

const scoped_refptr<base::RefCountedMemory>& Attachment::GetBytes() const {
  return bytes_;
}

Attachment::Attachment(const sync_pb::AttachmentId& id,
                       const scoped_refptr<base::RefCountedMemory>& bytes)
    : id_(id), bytes_(bytes) {
  DCHECK(!id.unique_id().empty());
  DCHECK(bytes);
}

// Static.
sync_pb::AttachmentId Attachment::CreateId() {
  sync_pb::AttachmentId result;
  // Only requirement here is that this id must be globally unique.
  result.set_unique_id(base::RandBytesAsString(16));
  return result;
}

}  // namespace syncer
