// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace sync_pb {
class AttachmentId;
}  // namespace sync_pb

namespace syncer {

class Attachment;

// A place to locally store and access Attachments.
class SYNC_EXPORT AttachmentStore {
 public:
  AttachmentStore();
  virtual ~AttachmentStore();

  // TODO(maniscalco): Consider udpating Read and Write methods to support
  // resumable transfers.

  enum Result {
    SUCCESS,            // No error.
    NOT_FOUND,          // Attachment was not found or does not exist.
    UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<void(const Result&, scoped_ptr<Attachment>)>
      ReadCallback;
  typedef base::Callback<void(const Result&, const sync_pb::AttachmentId& id)>
      WriteCallback;
  typedef base::Callback<void(const Result&)> DropCallback;

  // Asynchronously reads the attachment identified by |id|.
  //
  // |callback| will be invoked when finished. If the attachment does not exist,
  // |callback|'s Result will be NOT_FOUND and |callback|'s attachment will be
  // null.
  virtual void Read(const sync_pb::AttachmentId& id,
                    const ReadCallback& callback) = 0;

  // Asynchronously writes |bytes| to the store.
  //
  // |callback| will be invoked when finished.
  virtual void Write(const scoped_refptr<base::RefCountedMemory>& bytes,
                     const WriteCallback& callback) = 0;

  // Asynchronously drops the attchment with the given id from this store.
  //
  // This does not remove the attachment from the server. |callback| will be
  // invoked when finished. If the attachment does not exist, |callback|'s
  // Result will be NOT_FOUND.
  virtual void Drop(const sync_pb::AttachmentId& id,
                    const DropCallback& callback) = 0;
};

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
