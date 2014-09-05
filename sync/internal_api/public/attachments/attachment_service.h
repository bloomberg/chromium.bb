// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "sync/api/attachments/attachment.h"
#include "sync/base/sync_export.h"

namespace syncer {

class SyncData;

// AttachmentService is responsible for managing a model type's attachments.
//
// Outside of sync code, AttachmentService shouldn't be used directly. Instead
// use the functionality provided by SyncData and SyncChangeProcessor.
//
// Destroying this object does not necessarily cancel outstanding async
// operations. If you need cancel like semantics, use WeakPtr in the callbacks.
class SYNC_EXPORT AttachmentService {
 public:
  // The result of a GetOrDownloadAttachments operation.
  enum GetOrDownloadResult {
    GET_SUCCESS,            // No error, all attachments returned.
    GET_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<
      void(const GetOrDownloadResult&, scoped_ptr<AttachmentMap> attachments)>
      GetOrDownloadCallback;

  // The result of a DropAttachments operation.
  enum DropResult {
    DROP_SUCCESS,            // No error, all attachments dropped.
    DROP_UNSPECIFIED_ERROR,  // An unspecified error occurred. Some or all
                             // attachments may not have been dropped.
  };

  typedef base::Callback<void(const DropResult&)> DropCallback;

  // The result of a StoreAttachments operation.
  enum StoreResult {
    STORE_SUCCESS,            // No error, all attachments stored (at least
                              // locally).
    STORE_UNSPECIFIED_ERROR,  // An unspecified error occurred. Some or all
                              // attachments may not have been stored.
  };

  typedef base::Callback<void(const StoreResult&)> StoreCallback;

  // An interface that embedder code implements to be notified about different
  // events that originate from AttachmentService.
  // This interface will be called from the same thread AttachmentService was
  // created and called.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Attachment is uploaded to server and attachment_id is updated with server
    // url.
    virtual void OnAttachmentUploaded(const AttachmentId& attachment_id) = 0;
  };

  AttachmentService();
  virtual ~AttachmentService();

  // See SyncData::GetOrDownloadAttachments.
  virtual void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) = 0;

  // See SyncData::DropAttachments.
  virtual void DropAttachments(const AttachmentIdList& attachment_ids,
                               const DropCallback& callback) = 0;

  // Store |attachments| on device and (eventually) upload them to the server.
  //
  // Invokes |callback| once the attachments have been written to device
  // storage.
  virtual void StoreAttachments(const AttachmentList& attachments,
                                const StoreCallback& callback) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_SERVICE_H_
