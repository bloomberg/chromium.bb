// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/base/sync_export.h"

namespace base {
class RefCountedMemory;
}  // namespace base

namespace syncer {

class Attachment;
class AttachmentId;

// A place to locally store and access Attachments.
//
// Destroying this object does not necessarily cancel outstanding async
// operations. If you need cancel like semantics, use WeakPtr in the callbacks.
class SYNC_EXPORT AttachmentStore : public base::RefCounted<AttachmentStore> {
 public:
  AttachmentStore();

  // TODO(maniscalco): Consider udpating Read and Write methods to support
  // resumable transfers (bug 353292).

  enum Result {
    SUCCESS,            // No error, all completed successfully.
    UNSPECIFIED_ERROR,  // An unspecified error occurred for one or more items.
  };

  typedef base::Callback<void(const Result&,
                              scoped_ptr<AttachmentMap>,
                              scoped_ptr<AttachmentIdList>)> ReadCallback;
  typedef base::Callback<void(const Result&)> WriteCallback;
  typedef base::Callback<void(const Result&)> DropCallback;

  // Asynchronously reads the attachments identified by |ids|.
  //
  // |callback| will be invoked when finished. AttachmentStore will attempt to
  // read all attachments specified in ids. If any of the attachments do not
  // exist or could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  // Callback's AttachmentMap will contain all attachments that were
  // successfully read, AttachmentIdList will contain attachment ids of
  // attachments that are unavailable in attachment store, these need to be
  // downloaded from server.
  //
  // Reads on individual attachments are treated atomically; |callback| will not
  // read only part of an attachment.
  virtual void Read(const AttachmentIdList& ids,
                    const ReadCallback& callback) = 0;

  // Asynchronously writes |attachments| to the store.
  //
  // Will not overwrite stored attachments. Attempting to overwrite an
  // attachment that already exists is not an error.
  //
  // |callback| will be invoked when finished. If any of the attachments could
  // not be written |callback|'s Result will be UNSPECIFIED_ERROR. When this
  // happens, some or none of the attachments may have been written
  // successfully.
  virtual void Write(const AttachmentList& attachments,
                     const WriteCallback& callback) = 0;

  // Asynchronously drops |attchments| from this store.
  //
  // This does not remove attachments from the server.
  //
  // |callback| will be invoked when finished. Attempting to drop an attachment
  // that does not exist is not an error. If any of the existing attachment
  // could not be dropped, |callback|'s Result will be UNSPECIFIED_ERROR. When
  // this happens, some or none of the attachments may have been dropped
  // successfully.
  virtual void Drop(const AttachmentIdList& ids,
                    const DropCallback& callback) = 0;

 protected:
  friend class base::RefCounted<AttachmentStore>;
  virtual ~AttachmentStore();
};

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_STORE_H_
