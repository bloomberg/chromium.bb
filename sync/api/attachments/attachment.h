// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/base/sync_export.h"

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
  static Attachment Create(const scoped_refptr<base::RefCountedMemory>& data);

  // Creates an attachment with the supplied id, data and crc32c.
  //
  // Used when you want to recreate a specific attachment. E.g. creating a local
  // copy of an attachment that already exists on the sync server.
  static Attachment CreateFromParts(
      const AttachmentId& id,
      const scoped_refptr<base::RefCountedMemory>& data,
      uint32_t crc32c);

  // Returns this attachment's id.
  const AttachmentId& GetId() const;

  // Returns this attachment's data.
  const scoped_refptr<base::RefCountedMemory>& GetData() const;

  // Returns precomputed crc32c hash of data. In ideal case this hash is
  // computed when attachment is first created. It is then passed around through
  // local attachment store and attachment server. Crc is verified when
  // attachment is downloaded from server or loaded from local storage.
  uint32_t GetCrc32c() const;

 private:
  AttachmentId id_;
  scoped_refptr<base::RefCountedMemory> data_;
  uint32_t crc32c_;

  Attachment(const AttachmentId& id,
             const scoped_refptr<base::RefCountedMemory>& data,
             uint32_t crc32c);
};

typedef std::vector<syncer::Attachment> AttachmentList;
typedef std::map<AttachmentId, Attachment> AttachmentMap;

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_H_
