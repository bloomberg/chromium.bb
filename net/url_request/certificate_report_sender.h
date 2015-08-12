// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_CERTIFICATE_REPORT_SENDER_H_
#define NET_URL_REQUEST_CERTIFICATE_REPORT_SENDER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/http/transport_security_state.h"
#include "net/url_request/url_request.h"

class GURL;

namespace net {

class URLRequestContext;

// CertificateReportSender asynchronously sends serialized certificate
// reports to a URI. It takes serialized reports as a sequence of bytes
// so as to be agnostic to the format of the report being sent (JSON,
// protobuf, etc.) and the particular data that it contains. Multiple
// reports can be in-flight at once. This class owns inflight requests
// and cleans them up when necessary.
class NET_EXPORT CertificateReportSender
    : public URLRequest::Delegate,
      public TransportSecurityState::ReportSender {
 public:
  // Represents whether or not to send cookies along with reports.
  enum CookiesPreference { SEND_COOKIES, DO_NOT_SEND_COOKIES };

  // Constructs a CertificateReportSender that sends reports with the
  // given |request_context| and includes or excludes cookies based on
  // |cookies_preference|. |request_context| must outlive the
  // CertificateReportSender.
  CertificateReportSender(URLRequestContext* request_context,
                          CookiesPreference cookies_preference);

  ~CertificateReportSender() override;

  // TransportSecurityState::ReportSender implementation.
  void Send(const GURL& report_uri, const std::string& report) override;

  // net::URLRequest::Delegate implementation.
  void OnResponseStarted(URLRequest* request) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

 private:
  net::URLRequestContext* const request_context_;

  CookiesPreference cookies_preference_;

  // Owns the contained requests.
  std::set<URLRequest*> inflight_requests_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportSender);
};

}  // namespace net

#endif  // NET_URL_REQUEST_CERTIFICATE_REPORT_H_
