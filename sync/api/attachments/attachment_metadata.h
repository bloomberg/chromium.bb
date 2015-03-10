// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_METADATA_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_METADATA_H_

#include <vector>

#include "base/basictypes.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/base/sync_export.h"

namespace syncer {

// This class represents immutable Attachment metadata.
//
// It is OK to copy and return AttachmentMetadata by value.
class SYNC_EXPORT AttachmentMetadata {
 public:
  AttachmentMetadata(const AttachmentId& id, size_t size);
  ~AttachmentMetadata();

  // Default copy and assignment are welcome.

  // Returns this attachment's id.
  const AttachmentId& GetId() const;

  // Returns this attachment's size in bytes.
  size_t GetSize() const;

 private:
  // TODO(maniscalco): Reconcile AttachmentMetadata and
  // AttachmentId. AttachmentId knows the size of the attachment so
  // AttachmentMetadata may not be necessary (crbug/465375).
  AttachmentId id_;
  size_t size_;
};

typedef std::vector<syncer::AttachmentMetadata> AttachmentMetadataList;

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_METADATA_H_
