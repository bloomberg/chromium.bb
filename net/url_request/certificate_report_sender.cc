// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/certificate_report_sender.h"

#include <utility>

#include "base/stl_util.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace net {

CertificateReportSender::CertificateReportSender(
    URLRequestContext* request_context,
    CookiesPreference cookies_preference)
    : request_context_(request_context),
      cookies_preference_(cookies_preference) {}

CertificateReportSender::~CertificateReportSender() {
  // Cancel all of the uncompleted requests.
  STLDeleteElements(&inflight_requests_);
}

void CertificateReportSender::Send(const GURL& report_uri,
                                   const std::string& report) {
  scoped_ptr<URLRequest> url_request =
      request_context_->CreateRequest(report_uri, DEFAULT_PRIORITY, this);

  int load_flags =
      LOAD_BYPASS_CACHE | LOAD_DISABLE_CACHE | LOAD_DO_NOT_SEND_AUTH_DATA;
  if (cookies_preference_ != SEND_COOKIES) {
    load_flags |= LOAD_DO_NOT_SEND_COOKIES | LOAD_DO_NOT_SAVE_COOKIES;
  }
  url_request->SetLoadFlags(load_flags);

  url_request->set_method("POST");

  scoped_ptr<UploadElementReader> reader(
      UploadOwnedBytesElementReader::CreateWithString(report));
  url_request->set_upload(
      ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));

  URLRequest* raw_url_request = url_request.get();
  inflight_requests_.insert(url_request.release());
  raw_url_request->Start();
}

void CertificateReportSender::OnResponseStarted(URLRequest* request) {
  // TODO(estark): call a callback so that the caller can print a
  // warning on failure.
  DVLOG(1) << "Failed to send certificate report for " << request->url().host();

  CHECK_GT(inflight_requests_.erase(request), 0u);
  // Clean up the request, which cancels it.
  delete request;
}

void CertificateReportSender::OnReadCompleted(URLRequest* request,
                                              int bytes_read) {
  NOTREACHED();
}

}  // namespace net
