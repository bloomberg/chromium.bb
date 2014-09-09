// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

URLRequestTestJob* job_a;

URLRequestJob* FactoryA(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_a = new URLRequestTestJob(request, network_delegate);
  return job_a;
}

URLRequestTestJob* job_b;

URLRequestJob* FactoryB(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_b = new URLRequestTestJob(request, network_delegate);
  return job_b;
}

URLRequestTestJob* job_c;

class TestURLRequestInterceptor : public URLRequestInterceptor {
 public:
  virtual ~TestURLRequestInterceptor() {}

  virtual URLRequestJob* MaybeInterceptRequest(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    job_c = new URLRequestTestJob(request, network_delegate);
    return job_c;
  }
};

TEST(URLRequestFilter, BasicMatching) {
  TestDelegate delegate;
  TestURLRequestContext request_context;
  URLRequestFilter* filter = URLRequestFilter::GetInstance();

  const GURL kUrl1("http://foo.com/");
  scoped_ptr<URLRequest> request1(request_context.CreateRequest(
      kUrl1, DEFAULT_PRIORITY, &delegate, NULL));

  const GURL kUrl2("http://bar.com/");
  scoped_ptr<URLRequest> request2(request_context.CreateRequest(
      kUrl2, DEFAULT_PRIORITY, &delegate, NULL));

  // Check AddUrlHandler checks for invalid URLs.
  EXPECT_FALSE(filter->AddUrlHandler(GURL(), &FactoryA));

  // Check URL matching.
  filter->ClearHandlers();
  EXPECT_TRUE(filter->AddUrlHandler(kUrl1, &FactoryA));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request1.get(), NULL);
    EXPECT_EQ(job_a, found.get());
    EXPECT_TRUE(job_a != NULL);
    job_a = NULL;
  }
  EXPECT_EQ(filter->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_TRUE(filter->MaybeInterceptRequest(request2.get(), NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove URL matching.
  filter->RemoveUrlHandler(kUrl1);
  EXPECT_TRUE(filter->MaybeInterceptRequest(request1.get(), NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddHostnameHandler(kUrl1.scheme(), kUrl1.host(), &FactoryB);
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request1.get(), NULL);
    EXPECT_EQ(job_b, found.get());
    EXPECT_TRUE(job_b != NULL);
    job_b = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check we don't match other hostnames.
  EXPECT_TRUE(filter->MaybeInterceptRequest(request2.get(), NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check we can remove hostname matching.
  filter->RemoveHostnameHandler(kUrl1.scheme(), kUrl1.host());
  EXPECT_TRUE(filter->MaybeInterceptRequest(request1.get(), NULL) == NULL);
  EXPECT_EQ(1, filter->hit_count());

  // Check URLRequestInterceptor hostname matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddHostnameInterceptor(
      kUrl1.scheme(), kUrl1.host(),
      scoped_ptr<net::URLRequestInterceptor>(new TestURLRequestInterceptor()));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request1.get(), NULL);
    EXPECT_EQ(job_c, found.get());
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  // Check URLRequestInterceptor URL matching.
  filter->ClearHandlers();
  EXPECT_EQ(0, filter->hit_count());
  filter->AddUrlInterceptor(
      kUrl2,
      scoped_ptr<net::URLRequestInterceptor>(new TestURLRequestInterceptor()));
  {
    scoped_refptr<URLRequestJob> found =
        filter->MaybeInterceptRequest(request2.get(), NULL);
    EXPECT_EQ(job_c, found.get());
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, filter->hit_count());

  filter->ClearHandlers();
}

}  // namespace

}  // namespace net
