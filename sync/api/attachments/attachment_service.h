// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_SERVICE_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_SERVICE_H_

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
    DROP_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<void(const DropResult&)> DropCallback;

  AttachmentService();
  virtual ~AttachmentService();

  // See SyncData::GetOrDownloadAttachments.
  virtual void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) = 0;

  // See SyncData::DropAttachments.
  virtual void DropAttachments(const AttachmentIdList& attachment_ids,
                               const DropCallback& callback) = 0;

  // This method should be called when a SyncData is about to be added to the
  // sync database so we have a chance to persist the Attachment locally and
  // schedule it for upload to the sync server.
  virtual void OnSyncDataAdd(const SyncData& sync_data) = 0;

  // This method should be called when a SyncData is about to be deleted from
  // the sync database so we can remove any unreferenced attachments from local
  // storage.
  virtual void OnSyncDataDelete(const SyncData& sync_data) = 0;

  // This method should be called when a SyncData is about to be updated so we
  // can remove unreferenced attachments from local storage and ensure new
  // attachments are persisted and uploaded to the sync server.
  virtual void OnSyncDataUpdate(const AttachmentIdList& old_attachment_ids,
                                const SyncData& updated_sync_data) = 0;
};

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_SERVICE_H_
