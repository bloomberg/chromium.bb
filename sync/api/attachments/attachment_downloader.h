// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "sync/api/attachments/attachment.h"
#include "sync/base/sync_export.h"

namespace syncer {

// AttachmentDownloader is responsible for downloading attachments from server.
class SYNC_EXPORT AttachmentDownloader {
 public:
  // The result of a DownloadAttachment operation.
  enum DownloadResult {
    DOWNLOAD_SUCCESS,            // No error, attachment was downloaded
                                 // successfully.
    DOWNLOAD_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<void(const DownloadResult&, scoped_ptr<Attachment>)>
      DownloadCallback;

  virtual ~AttachmentDownloader();

  // Download attachment referred by |attachment_id| and invoke |callback| when
  // done.
  //
  // |callback| will receive a DownloadResult code and an Attachment object. If
  // DownloadResult is not DOWNLOAD_SUCCESS then attachment pointer is NULL.
  virtual void DownloadAttachment(const AttachmentId& attachment_id,
                                  const DownloadCallback& callback) = 0;
};

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_
