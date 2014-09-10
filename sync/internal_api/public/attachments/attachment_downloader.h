// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "sync/api/attachments/attachment.h"
#include "sync/base/sync_export.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

// AttachmentDownloader is responsible for downloading attachments from server.
class SYNC_EXPORT AttachmentDownloader {
 public:
  // The result of a DownloadAttachment operation.
  enum DownloadResult {
    DOWNLOAD_SUCCESS,            // No error, attachment was downloaded
                                 // successfully.
    DOWNLOAD_TRANSIENT_ERROR,    // A transient error occurred, try again later.
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

  // Create instance of AttachmentDownloaderImpl.
  // |sync_service_url| is the URL of the sync service.
  //
  // |url_request_context_getter| provides a URLRequestContext.
  //
  // |account_id| is the account id to use for downloads.
  //
  // |scopes| is the set of scopes to use for downloads.
  //
  // |token_service_provider| provides an OAuth2 token service.
  static scoped_ptr<AttachmentDownloader> Create(
      const GURL& sync_service_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet scopes,
      const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
          token_service_provider);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_H_
