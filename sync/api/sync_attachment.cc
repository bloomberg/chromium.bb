// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_attachment.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/rand_util.h"

namespace syncer {

SyncAttachment::~SyncAttachment() {}

// Static.
scoped_ptr<SyncAttachment> SyncAttachment::Create(
    const scoped_refptr<base::RefCountedMemory>& bytes) {
  sync_pb::SyncAttachmentId id;
  // Only requirement here is that this id must be globally unique.
  id.set_unique_id(base::RandBytesAsString(16));
  return CreateWithId(id, bytes);
}

// Static.
scoped_ptr<SyncAttachment> SyncAttachment::CreateWithId(
    const sync_pb::SyncAttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& bytes) {
  return scoped_ptr<SyncAttachment>(new SyncAttachment(id, bytes)).Pass();
}

const sync_pb::SyncAttachmentId& SyncAttachment::GetId() const { return id_; }

const scoped_refptr<base::RefCountedMemory>& SyncAttachment::GetBytes() const {
  return bytes_;
}

SyncAttachment::SyncAttachment(
    const sync_pb::SyncAttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& bytes)
    : id_(id), bytes_(bytes) {
  DCHECK(!id.unique_id().empty());
  DCHECK(bytes);
}

}  // namespace syncer
