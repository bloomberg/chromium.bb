// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_SYNC_ATTACHMENT_H_
#define SYNC_API_SYNC_ATTACHMENT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace syncer {

// An immutable blob of in-memory data attached to a sync item.
class SYNC_EXPORT SyncAttachment {
 public:
  ~SyncAttachment();

  // Creates an attachment with a unique id and the supplied bytes.
  //
  // Used when creating a brand new attachment.
  static scoped_ptr<SyncAttachment> Create(
      const scoped_refptr<base::RefCountedMemory>& bytes);

  // Creates an attachment with the supplied id and bytes.
  //
  // Used when you want to recreate a specific attachment. E.g. creating a local
  // copy of an attachment that already exists on the sync server.
  static scoped_ptr<SyncAttachment> CreateWithId(
      const sync_pb::SyncAttachmentId& id,
      const scoped_refptr<base::RefCountedMemory>& bytes);

  // Returns this attachment's id.
  const sync_pb::SyncAttachmentId& GetId() const;

  // Returns this attachment's bytes.
  const scoped_refptr<base::RefCountedMemory>& GetBytes() const;

 private:
  const sync_pb::SyncAttachmentId id_;
  const scoped_refptr<base::RefCountedMemory> bytes_;

  SyncAttachment(const sync_pb::SyncAttachmentId& id,
                 const scoped_refptr<base::RefCountedMemory>& bytes);

  // Default copy ctor welcome.
  DISALLOW_ASSIGN(SyncAttachment);
};

}  // namespace syncer

#endif  // SYNC_API_SYNC_ATTACHMENT_H_
