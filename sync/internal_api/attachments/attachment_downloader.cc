// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_downloader.h"

#include "sync/internal_api/public/attachments/attachment_downloader_impl.h"

namespace syncer {

AttachmentDownloader::~AttachmentDownloader() {
}

// Factory function for creating AttachmentDownloaderImpl.
// It is introduced to avoid SYNC_EXPORT-ing AttachmentDownloaderImpl since it
// inherits from OAuth2TokenService::Consumer which is not exported.
scoped_ptr<AttachmentDownloader> AttachmentDownloader::Create(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet scopes,
    scoped_ptr<OAuth2TokenServiceRequest::TokenServiceProvider>
        token_service_provider) {
  return scoped_ptr<AttachmentDownloader>(
      new AttachmentDownloaderImpl(sync_service_url,
                                   url_request_context_getter,
                                   account_id,
                                   scopes,
                                   token_service_provider.Pass()));
}

}  // namespace syncer
