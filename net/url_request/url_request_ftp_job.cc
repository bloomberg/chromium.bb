// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"

namespace net {

URLRequestFtpJob::URLRequestFtpJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    FtpTransactionFactory* ftp_transaction_factory,
    FtpAuthCache* ftp_auth_cache)
    : URLRequestJob(request, network_delegate),
      pac_request_(NULL),
      response_info_(NULL),
      read_in_progress_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      ftp_transaction_factory_(ftp_transaction_factory),
      ftp_auth_cache_(ftp_auth_cache) {
  DCHECK(ftp_transaction_factory);
  DCHECK(ftp_auth_cache);
}

// static
URLRequestJob* URLRequestFtpJob::Factory(URLRequest* request,
                                         NetworkDelegate* network_delegate,
                                         const std::string& scheme) {
  DCHECK_EQ(scheme, "ftp");

  int port = request->url().IntPort();
  if (request->url().has_port() &&
      !IsPortAllowedByFtp(port) && !IsPortAllowedByOverride(port)) {
    return new URLRequestErrorJob(request,
                                  network_delegate,
                                  ERR_UNSAFE_PORT);
  }

  return new URLRequestFtpJob(request,
                              network_delegate,
                              request->context()->ftp_transaction_factory(),
                              request->context()->ftp_auth_cache());
}

bool URLRequestFtpJob::IsSafeRedirect(const GURL& location) {
  // Disallow all redirects.
  return false;
}

bool URLRequestFtpJob::GetMimeType(std::string* mime_type) const {
  if (proxy_info_.is_direct()) {
    if (ftp_transaction_->GetResponseInfo()->is_directory_listing) {
      *mime_type = "text/vnd.chromium.ftp-dir";
      return true;
    }
  } else {
    // No special handling of MIME type is needed. As opposed to direct FTP
    // transaction, we do not get a raw directory listing to parse.
    return http_transaction_->GetResponseInfo()->
        headers->GetMimeType(mime_type);
  }
  return false;
}

void URLRequestFtpJob::GetResponseInfo(HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

HostPortPair URLRequestFtpJob::GetSocketAddress() const {
  if (proxy_info_.is_direct()) {
    if (!ftp_transaction_)
      return HostPortPair();
    return ftp_transaction_->GetResponseInfo()->socket_address;
  } else {
    if (!http_transaction_)
      return HostPortPair();
    return http_transaction_->GetResponseInfo()->socket_address;
  }
}

URLRequestFtpJob::~URLRequestFtpJob() {
  if (pac_request_)
    request_->context()->proxy_service()->CancelPacRequest(pac_request_);
}

void URLRequestFtpJob::OnResolveProxyComplete(int result) {
  pac_request_ = NULL;

  if (result != OK) {
    OnStartCompletedAsync(result);
    return;
  }

  // Remove unsupported proxies from the list.
  proxy_info_.RemoveProxiesWithoutScheme(
      ProxyServer::SCHEME_DIRECT |
      ProxyServer::SCHEME_HTTP |
      ProxyServer::SCHEME_HTTPS);

  // TODO(phajdan.jr): Implement proxy fallback, http://crbug.com/171495 .
  if (proxy_info_.is_direct())
    StartFtpTransaction();
  else if (proxy_info_.is_http() || proxy_info_.is_https())
    StartHttpTransaction();
  else
    OnStartCompletedAsync(ERR_NO_SUPPORTED_PROXIES);
}

void URLRequestFtpJob::StartFtpTransaction() {
  // Create a transaction.
  DCHECK(!ftp_transaction_);

  ftp_request_info_.url = request_->url();
  ftp_transaction_.reset(ftp_transaction_factory_->CreateTransaction());

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  int rv;
  if (ftp_transaction_) {
    rv = ftp_transaction_->Start(
        &ftp_request_info_,
        base::Bind(&URLRequestFtpJob::OnStartCompleted,
                   base::Unretained(this)),
        request_->net_log());
    if (rv == ERR_IO_PENDING)
      return;
  } else {
    rv = ERR_FAILED;
  }
  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  OnStartCompletedAsync(rv);
}

void URLRequestFtpJob::StartHttpTransaction() {
  // Create a transaction.
  DCHECK(!http_transaction_);

  // Do not cache FTP responses sent through HTTP proxy.
  // Do not send HTTP auth data because this is really FTP.
  request_->set_load_flags(request_->load_flags() |
                           LOAD_DISABLE_CACHE |
                           LOAD_DO_NOT_SAVE_COOKIES |
                           LOAD_DO_NOT_SEND_AUTH_DATA |
                           LOAD_DO_NOT_SEND_COOKIES);

  http_request_info_.url = request_->url();
  http_request_info_.method = request_->method();
  http_request_info_.load_flags = request_->load_flags();
  http_request_info_.priority = request_->priority();
  http_request_info_.request_id = request_->identifier();

  int rv = request_->context()->http_transaction_factory()->CreateTransaction(
      &http_transaction_, NULL);
  if (rv == OK) {
    rv = http_transaction_->Start(
        &http_request_info_,
        base::Bind(&URLRequestFtpJob::OnStartCompleted,
                   base::Unretained(this)),
        request_->net_log());
    if (rv == ERR_IO_PENDING)
      return;
  }
  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  OnStartCompletedAsync(rv);
}

void URLRequestFtpJob::OnStartCompleted(int result) {
  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  // Note that ftp_transaction_ may be NULL due to a creation failure.
  if (ftp_transaction_) {
    // FTP obviously doesn't have HTTP Content-Length header. We have to pass
    // the content size information manually.
    set_expected_content_size(
        ftp_transaction_->GetResponseInfo()->expected_content_size);
  }

  if (result == OK) {
    if (http_transaction_)
      response_info_ = http_transaction_->GetResponseInfo();
    NotifyHeadersComplete();
  } else if (ftp_transaction_ &&
             ftp_transaction_->GetResponseInfo()->needs_auth) {
    GURL origin = request_->url().GetOrigin();
    if (server_auth_ && server_auth_->state == AUTH_STATE_HAVE_AUTH) {
      ftp_auth_cache_->Remove(origin, server_auth_->credentials);
    } else if (!server_auth_) {
      server_auth_ = new AuthData();
    }
    server_auth_->state = AUTH_STATE_NEED_AUTH;

    FtpAuthCache::Entry* cached_auth = ftp_auth_cache_->Lookup(origin);
    if (cached_auth) {
      // Retry using cached auth data.
      SetAuth(cached_auth->credentials);
    } else {
      // Prompt for a username/password.
      NotifyHeadersComplete();
    }
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestFtpJob::OnStartCompletedAsync(int result) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestFtpJob::OnStartCompleted,
                 weak_factory_.GetWeakPtr(), result));
}

void URLRequestFtpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;
  if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }
  NotifyReadComplete(result);
}

void URLRequestFtpJob::RestartTransactionWithAuth() {
  DCHECK(ftp_transaction_);
  DCHECK(server_auth_ && server_auth_->state == AUTH_STATE_HAVE_AUTH);

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = ftp_transaction_->RestartWithAuth(
      server_auth_->credentials,
      base::Bind(&URLRequestFtpJob::OnStartCompleted,
                 base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  OnStartCompletedAsync(rv);
}

void URLRequestFtpJob::Start() {
  DCHECK(!pac_request_);
  DCHECK(!ftp_transaction_);
  DCHECK(!http_transaction_);

  int rv = OK;
  if (request_->load_flags() & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
  } else {
    rv = request_->context()->proxy_service()->ResolveProxy(
        request_->url(),
        &proxy_info_,
        base::Bind(&URLRequestFtpJob::OnResolveProxyComplete,
                   base::Unretained(this)),
        &pac_request_,
        request_->net_log());

    if (rv == ERR_IO_PENDING)
      return;
  }
  OnResolveProxyComplete(rv);
}

void URLRequestFtpJob::Kill() {
  if (ftp_transaction_)
    ftp_transaction_.reset();
  if (http_transaction_)
    http_transaction_.reset();
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

LoadState URLRequestFtpJob::GetLoadState() const {
  if (proxy_info_.is_direct()) {
    return ftp_transaction_ ?
        ftp_transaction_->GetLoadState() : LOAD_STATE_IDLE;
  } else {
    return http_transaction_ ?
        http_transaction_->GetLoadState() : LOAD_STATE_IDLE;
  }
}

bool URLRequestFtpJob::NeedsAuth() {
  // TODO(phajdan.jr): Implement proxy auth, http://crbug.com/171497 .
  if (!ftp_transaction_)
    return false;

  // Note that we only have to worry about cases where an actual FTP server
  // requires auth (and not a proxy), because connecting to FTP via proxy
  // effectively means the browser communicates via HTTP, and uses HTTP's
  // Proxy-Authenticate protocol when proxy servers require auth.
  return server_auth_ && server_auth_->state == AUTH_STATE_NEED_AUTH;
}

void URLRequestFtpJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* result) {
  DCHECK((server_auth_ != NULL) &&
         (server_auth_->state == AUTH_STATE_NEED_AUTH));
  scoped_refptr<AuthChallengeInfo> auth_info(new AuthChallengeInfo);
  auth_info->is_proxy = false;
  auth_info->challenger = HostPortPair::FromURL(request_->url());
  // scheme and realm are kept empty.
  DCHECK(auth_info->scheme.empty());
  DCHECK(auth_info->realm.empty());
  result->swap(auth_info);
}

void URLRequestFtpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(ftp_transaction_);
  DCHECK(NeedsAuth());
  server_auth_->state = AUTH_STATE_HAVE_AUTH;
  server_auth_->credentials = credentials;

  ftp_auth_cache_->Add(request_->url().GetOrigin(), server_auth_->credentials);

  RestartTransactionWithAuth();
}

void URLRequestFtpJob::CancelAuth() {
  DCHECK(ftp_transaction_);
  DCHECK(NeedsAuth());
  server_auth_->state = AUTH_STATE_CANCELED;

  // Once the auth is cancelled, we proceed with the request as though
  // there were no auth.  Schedule this for later so that we don't cause
  // any recursing into the caller as a result of this call.
  OnStartCompletedAsync(OK);
}

UploadProgress URLRequestFtpJob::GetUploadProgress() const {
  return UploadProgress();
}

bool URLRequestFtpJob::ReadRawData(IOBuffer* buf,
                                   int buf_size,
                                   int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv;
  if (proxy_info_.is_direct()) {
    rv = ftp_transaction_->Read(buf, buf_size,
                                base::Bind(&URLRequestFtpJob::OnReadCompleted,
                                           base::Unretained(this)));
  } else {
    rv = http_transaction_->Read(buf, buf_size,
                                 base::Bind(&URLRequestFtpJob::OnReadCompleted,
                                            base::Unretained(this)));
  }

  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }

  if (rv == ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
  return false;
}

}  // namespace net
