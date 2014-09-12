// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_failed_job.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"

namespace net {
namespace {

const char kMockHostname[] = "mock.failed.request";

// Gets the numeric net error code from URL of the form:
// scheme://kMockHostname/error_code.  The error code must be a valid
// net error code, and not net::OK or net::ERR_IO_PENDING.
int GetErrorCode(net::URLRequest* request) {
  int net_error;
  std::string path = request->url().path();
  if (path[0] == '/' && base::StringToInt(path.c_str() + 1, &net_error)) {
    CHECK_LT(net_error, 0);
    CHECK_NE(net_error, net::ERR_IO_PENDING);
    return net_error;
  }
  NOTREACHED();
  return net::ERR_UNEXPECTED;
}

GURL GetMockUrl(const std::string& scheme,
                const std::string& hostname,
                int net_error) {
  CHECK_LT(net_error, 0);
  CHECK_NE(net_error, net::ERR_IO_PENDING);
  return GURL(scheme + "://" + hostname + "/" + base::IntToString(net_error));
}

}  // namespace

URLRequestFailedJob::URLRequestFailedJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         int net_error)
    : net::URLRequestJob(request, network_delegate),
      net_error_(net_error),
      weak_factory_(this) {}

void URLRequestFailedJob::Start() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestFailedJob::StartAsync, weak_factory_.GetWeakPtr()));
}

// static
void URLRequestFailedJob::AddUrlHandler() {
  return AddUrlHandlerForHostname(kMockHostname);
}

// static
void URLRequestFailedJob::AddUrlHandlerForHostname(
    const std::string& hostname) {
  // Add |hostname| to net::URLRequestFilter for HTTP and HTTPS.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameHandler("http", hostname, URLRequestFailedJob::Factory);
  filter->AddHostnameHandler("https", hostname, URLRequestFailedJob::Factory);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrl(int net_error) {
  return GetMockHttpUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrl(int net_error) {
  return GetMockHttpsUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrlForHostname(
    int net_error, const std::string& hostname) {
  return GetMockUrl("http", hostname, net_error);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrlForHostname(
    int net_error, const std::string& hostname) {
  return GetMockUrl("https", hostname, net_error);
}

URLRequestFailedJob::~URLRequestFailedJob() {}

// static
net::URLRequestJob* URLRequestFailedJob::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new URLRequestFailedJob(
      request, network_delegate, GetErrorCode(request));
}

void URLRequestFailedJob::StartAsync() {
  NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                         net_error_));
}

}  // namespace net
