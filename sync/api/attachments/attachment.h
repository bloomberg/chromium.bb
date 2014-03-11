// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

// A blob of in-memory data attached to a sync item.
//
// While Attachment objects themselves aren't immutable (they are assignable)
// the data they wrap is immutable.
//
// It is cheap to copy Attachments. Feel free to store and return by value.
class SYNC_EXPORT Attachment {
 public:
  ~Attachment();

  // Default copy and assignment are welcome.

  // Creates an attachment with a unique id and the supplied data.
  //
  // Used when creating a brand new attachment.
  static scoped_ptr<Attachment> Create(
      const scoped_refptr<base::RefCountedMemory>& data);

  // Creates an attachment with the supplied id and data.
  //
  // Used when you want to recreate a specific attachment. E.g. creating a local
  // copy of an attachment that already exists on the sync server.
  static scoped_ptr<Attachment> CreateWithId(
      const sync_pb::AttachmentId& id,
      const scoped_refptr<base::RefCountedMemory>& data);

  // Returns this attachment's id.
  const sync_pb::AttachmentId& GetId() const;

  // Returns this attachment's data.
  const scoped_refptr<base::RefCountedMemory>& GetData() const;

 private:
  sync_pb::AttachmentId id_;
  scoped_refptr<base::RefCountedMemory> data_;

  friend class AttachmentTest;
  FRIEND_TEST_ALL_PREFIXES(AttachmentTest, CreateId_UniqueIdIsUnique);

  Attachment(const sync_pb::AttachmentId& id,
             const scoped_refptr<base::RefCountedMemory>& data);

  // Creates a unique attachment id.
  static sync_pb::AttachmentId CreateId();
};

typedef std::vector<syncer::Attachment> AttachmentList;
typedef std::string AttachmentIdUniqueId;  // AttachmentId.unique_id()
typedef std::map<AttachmentIdUniqueId, Attachment> AttachmentMap;
typedef std::vector<sync_pb::AttachmentId> AttachmentIdList;

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_H_
