// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_mock_data_job.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_filter.h"

namespace net {
namespace {

const char kMockHostname[] = "mock.data";

// Gets the data from URL of the form:
// scheme://kMockHostname/?data=abc&repeat_count=nnn.
std::string GetDataFromRequest(const net::URLRequest& request) {
  std::string value;
  if (!GetValueForKeyInQuery(request.url(), "data", &value))
    return "default_data";
  return value;
}

// Gets the numeric repeat count from URL of the form:
// scheme://kMockHostname/?data=abc&repeat_count=nnn.
int GetRepeatCountFromRequest(const net::URLRequest& request) {
  std::string value;
  if (!GetValueForKeyInQuery(request.url(), "repeat", &value))
    return 1;

  int repeat_count;
  if (!base::StringToInt(value, &repeat_count))
    return 1;

  DCHECK_GT(repeat_count, 0);

  return repeat_count;
}

GURL GetMockUrl(const std::string& scheme,
                const std::string& hostname,
                const std::string& data,
                int data_repeat_count) {
  DCHECK_GT(data_repeat_count, 0);
  std::string url(scheme + "://" + hostname + "/");
  url.append("?data=");
  url.append(data);
  url.append("&repeat=");
  url.append(base::IntToString(data_repeat_count));
  return GURL(url);
}

class MockJobInterceptor : public net::URLRequestInterceptor {
 public:
  MockJobInterceptor() {}
  ~MockJobInterceptor() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestMockDataJob(request, network_delegate,
                                     GetDataFromRequest(*request),
                                     GetRepeatCountFromRequest(*request));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

}  // namespace

URLRequestMockDataJob::URLRequestMockDataJob(URLRequest* request,
                                             NetworkDelegate* network_delegate,
                                             const std::string& data,
                                             int data_repeat_count)
    : URLRequestJob(request, network_delegate),
      data_offset_(0),
      weak_factory_(this) {
  DCHECK_GT(data_repeat_count, 0);
  for (int i = 0; i < data_repeat_count; ++i) {
    data_.append(data);
  }
}

void URLRequestMockDataJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&URLRequestMockDataJob::StartAsync,
                            weak_factory_.GetWeakPtr()));
}

URLRequestMockDataJob::~URLRequestMockDataJob() {
}

bool URLRequestMockDataJob::ReadRawData(IOBuffer* buf,
                                        int buf_size,
                                        int* bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = static_cast<int>(
      std::min(static_cast<size_t>(buf_size), data_.length() - data_offset_));
  memcpy(buf->data(), data_.c_str() + data_offset_, *bytes_read);
  data_offset_ += *bytes_read;
  return true;
}

void URLRequestMockDataJob::StartAsync() {
  if (!request_)
    return;

  set_expected_content_size(data_.length());
  NotifyHeadersComplete();
}

// static
void URLRequestMockDataJob::AddUrlHandler() {
  return AddUrlHandlerForHostname(kMockHostname);
}

// static
void URLRequestMockDataJob::AddUrlHandlerForHostname(
    const std::string& hostname) {
  // Add |hostname| to net::URLRequestFilter for HTTP and HTTPS.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor("http", hostname,
                                 make_scoped_ptr(new MockJobInterceptor()));
  filter->AddHostnameInterceptor("https", hostname,
                                 make_scoped_ptr(new MockJobInterceptor()));
}

// static
GURL URLRequestMockDataJob::GetMockHttpUrl(const std::string& data,
                                           int repeat_count) {
  return GetMockHttpUrlForHostname(kMockHostname, data, repeat_count);
}

// static
GURL URLRequestMockDataJob::GetMockHttpsUrl(const std::string& data,
                                            int repeat_count) {
  return GetMockHttpsUrlForHostname(kMockHostname, data, repeat_count);
}

// static
GURL URLRequestMockDataJob::GetMockHttpUrlForHostname(
    const std::string& hostname,
    const std::string& data,
    int repeat_count) {
  return GetMockUrl("http", hostname, data, repeat_count);
}

// static
GURL URLRequestMockDataJob::GetMockHttpsUrlForHostname(
    const std::string& hostname,
    const std::string& data,
    int repeat_count) {
  return GetMockUrl("https", hostname, data, repeat_count);
}

}  // namespace net
