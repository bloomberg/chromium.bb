// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"
#include "url/gurl.h"

namespace net {

// This class simulates a URLRequestJob failing with a given error code while
// trying to connect.
class URLRequestFailedJob : public URLRequestJob {
 public:
  URLRequestFailedJob(URLRequest* request,
                      NetworkDelegate* network_delegate,
                      int net_error);

  virtual void Start() OVERRIDE;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();
  static void AddUrlHandlerForHostname(const std::string& hostname);

  // Given a net error code, constructs a mock URL that will return that error
  // asynchronously when started.  |net_error| must be a valid net error code
  // other than net::OK and net::ERR_IO_PENDING.
  static GURL GetMockHttpUrl(int net_error);
  static GURL GetMockHttpsUrl(int net_error);

  // URLRequestFailedJob must be added as a handler for |hostname| for
  // the returned URL to return |net_error|.
  static GURL GetMockHttpUrlForHostname(int net_error,
                                        const std::string& hostname);
  static GURL GetMockHttpsUrlForHostname(int net_error,
                                         const std::string& hostname);

 protected:
  virtual ~URLRequestFailedJob();

 private:
  static URLRequestJob* Factory(URLRequest* request,
                                NetworkDelegate* network_delegate,
                                const std::string& scheme);

  // Simulate a failure.
  void StartAsync();

  const int net_error_;

  base::WeakPtrFactory<URLRequestFailedJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFailedJob);
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_
