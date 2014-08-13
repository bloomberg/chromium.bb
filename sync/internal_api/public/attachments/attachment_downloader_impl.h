// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_IMPL_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_IMPL_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/threading/non_thread_safe.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/internal_api/public/attachments/attachment_downloader.h"
#include "url/gurl.h"

namespace syncer {

// An implementation of AttachmentDownloader.
class AttachmentDownloaderImpl : public AttachmentDownloader,
                                 public OAuth2TokenService::Consumer,
                                 public net::URLFetcherDelegate,
                                 public base::NonThreadSafe {
 public:
  // |sync_service_url| is the URL of the sync service.
  //
  // |url_request_context_getter| provides a URLRequestContext.
  //
  // |account_id| is the account id to use for downloads.
  //
  // |scopes| is the set of scopes to use for downloads.
  //
  // |token_service_provider| provides an OAuth2 token service.
  AttachmentDownloaderImpl(
      const GURL& sync_service_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
          token_service_provider);
  virtual ~AttachmentDownloaderImpl();

  // AttachmentDownloader implementation.
  virtual void DownloadAttachment(const AttachmentId& attachment_id,
                                  const DownloadCallback& callback) OVERRIDE;

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  struct DownloadState;
  typedef std::string AttachmentUrl;
  typedef base::ScopedPtrHashMap<AttachmentUrl, DownloadState> StateMap;
  typedef std::vector<DownloadState*> StateList;

  scoped_ptr<net::URLFetcher> CreateFetcher(const AttachmentUrl& url,
                                            const std::string& access_token);
  void RequestAccessToken(DownloadState* download_state);
  void ReportResult(
      const DownloadState& download_state,
      const DownloadResult& result,
      const scoped_refptr<base::RefCountedString>& attachment_data);

  GURL sync_service_url_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::string account_id_;
  OAuth2TokenService::ScopeSet oauth2_scopes_;
  scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
      token_service_provider_;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  StateMap state_map_;
  // |requests_waiting_for_access_token_| only keeps references to DownloadState
  // objects while access token request is pending. It doesn't control objects'
  // lifetime.
  StateList requests_waiting_for_access_token_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentDownloaderImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_ATTACHMENT_DOWNLOADER_IMPL_H_
