// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REPORT_SENDER_H_
#define NET_URL_REQUEST_REPORT_SENDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/http/transport_security_state.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

class GURL;

namespace net {

class URLRequestContext;

// ReportSender asynchronously sends serialized reports to a URI.
// It takes serialized reports as a sequence of bytes so as to be agnostic to
// the format of the report being sent (JSON, protobuf, etc.) and the particular
// data that it contains. Multiple reports can be in-flight at once. This class
// owns inflight requests and cleans them up when necessary.
class NET_EXPORT ReportSender
    : public URLRequest::Delegate,
      public TransportSecurityState::ReportSenderInterface {
 public:
  static const int kLoadFlags;

  using SuccessCallback = base::Callback<void()>;
  using ErrorCallback = base::Callback<
      void(const GURL&, int /* net_error */, int /* http_response_code */)>;

  // Constructs a ReportSender that sends reports with the
  // given |request_context|, always excluding cookies. |request_context| must
  // outlive the ReportSender.
  explicit ReportSender(URLRequestContext* request_context,
                        net::NetworkTrafficAnnotationTag traffic_annotation);

  ~ReportSender() override;

  // TransportSecurityState::ReportSenderInterface implementation.
  void Send(const GURL& report_uri,
            base::StringPiece content_type,
            base::StringPiece report,
            const SuccessCallback& success_callback,
            const ErrorCallback& error_callback) override;

  // net::URLRequest::Delegate implementation.
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

 private:
  net::URLRequestContext* const request_context_;
  std::map<URLRequest*, std::unique_ptr<URLRequest>> inflight_requests_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(ReportSender);
};

}  // namespace net

#endif  // NET_URL_REQUEST_REPORT_SENDER_H_
