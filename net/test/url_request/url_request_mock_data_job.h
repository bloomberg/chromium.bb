// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_MOCK_DATA_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_MOCK_DATA_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequest;

// Mock data job, which synchronously returns data repeated multiple times.
class URLRequestMockDataJob : public URLRequestJob {
 public:
  URLRequestMockDataJob(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& data,
                        int data_repeat_count);

  void Start() override;
  bool ReadRawData(IOBuffer* buf, int buf_size, int* bytes_read) override;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();
  static void AddUrlHandlerForHostname(const std::string& hostname);

  // Given data and repeat cound, constructs a mock URL that will return that
  // data repeated |repeat_count| times when started.  |data| must be safe for
  // URL.
  static GURL GetMockHttpUrl(const std::string& data, int repeat_count);
  static GURL GetMockHttpsUrl(const std::string& data, int repeat_count);

  // URLRequestFailedJob must be added as a handler for |hostname| for
  // the returned URL to return |net_error|.
  static GURL GetMockHttpUrlForHostname(const std::string& hostname,
                                        const std::string& data,
                                        int repeat_count);
  static GURL GetMockHttpsUrlForHostname(const std::string& hostname,
                                         const std::string& data,
                                         int repeat_count);

 private:
  ~URLRequestMockDataJob() override;

  void StartAsync();

  std::string data_;
  size_t data_offset_;
  base::WeakPtrFactory<URLRequestMockDataJob> weak_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
