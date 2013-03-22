// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/base/auth.h"
#include "net/base/net_export.h"
#include "net/ftp/ftp_request_info.h"
#include "net/ftp/ftp_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_job.h"

namespace net {

class NetworkDelegate;
class FtpTransactionFactory;
class FtpAuthCache;

// A URLRequestJob subclass that is built on top of FtpTransaction. It
// provides an implementation for FTP.
class NET_EXPORT_PRIVATE URLRequestFtpJob : public URLRequestJob {
 public:
  URLRequestFtpJob(URLRequest* request,
                   NetworkDelegate* network_delegate,
                   FtpTransactionFactory* ftp_transaction_factory,
                   FtpAuthCache* ftp_auth_cache);

  // TODO(shalev): get rid of this function in favor of FtpProtocolHandler.
  static URLRequestJob* Factory(URLRequest* request,
                                NetworkDelegate* network_delegate,
                                const std::string& scheme);

 protected:
  virtual ~URLRequestFtpJob();

  // Overridden from URLRequestJob:
  virtual bool IsSafeRedirect(const GURL& location) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(HttpResponseInfo* info) OVERRIDE;
  virtual HostPortPair GetSocketAddress() const OVERRIDE;
  virtual void SetPriority(RequestPriority priority) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;

  RequestPriority priority() const { return priority_; }

 private:
  void OnResolveProxyComplete(int result);

  void StartFtpTransaction();
  void StartHttpTransaction();

  void OnStartCompleted(int result);
  void OnStartCompletedAsync(int result);
  void OnReadCompleted(int result);

  void RestartTransactionWithAuth();

  void LogFtpServerType(char server_type);

  // Overridden from URLRequestJob:
  virtual LoadState GetLoadState() const OVERRIDE;
  virtual bool NeedsAuth() OVERRIDE;
  virtual void GetAuthChallengeInfo(
      scoped_refptr<AuthChallengeInfo>* auth_info) OVERRIDE;
  virtual void SetAuth(const AuthCredentials& credentials) OVERRIDE;
  virtual void CancelAuth() OVERRIDE;

  // TODO(ibrar):  Yet to give another look at this function.
  virtual UploadProgress GetUploadProgress() const OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  RequestPriority priority_;

  ProxyInfo proxy_info_;
  ProxyService::PacRequest* pac_request_;

  FtpRequestInfo ftp_request_info_;
  scoped_ptr<FtpTransaction> ftp_transaction_;

  HttpRequestInfo http_request_info_;
  scoped_ptr<HttpTransaction> http_transaction_;
  const HttpResponseInfo* response_info_;

  bool read_in_progress_;

  scoped_refptr<AuthData> server_auth_;

  base::WeakPtrFactory<URLRequestFtpJob> weak_factory_;

  FtpTransactionFactory* ftp_transaction_factory_;
  FtpAuthCache* ftp_auth_cache_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFtpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
