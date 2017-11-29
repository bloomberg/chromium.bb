// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceRequest.h"

#include <memory>
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebURLRequest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ResourceRequestTest, CrossThreadResourceRequestData) {
  ResourceRequest original;
  original.SetURL(KURL("http://www.example.com/test.htm"));
  original.SetCacheMode(mojom::FetchCacheMode::kDefault);
  original.SetTimeoutInterval(10);
  original.SetSiteForCookies(KURL("http://www.example.com/first_party.htm"));
  original.SetRequestorOrigin(
      SecurityOrigin::Create(KURL("http://www.example.com/first_party.htm")));
  original.SetHTTPMethod(HTTPNames::GET);
  original.SetHTTPHeaderField(AtomicString("Foo"), AtomicString("Bar"));
  original.SetHTTPHeaderField(AtomicString("Piyo"), AtomicString("Fuga"));
  original.SetPriority(kResourceLoadPriorityLow, 20);

  scoped_refptr<EncodedFormData> original_body(
      EncodedFormData::Create("Test Body"));
  original.SetHTTPBody(original_body);
  original.SetAllowStoredCredentials(false);
  original.SetReportUploadProgress(false);
  original.SetHasUserGesture(false);
  original.SetDownloadToFile(false);
  original.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kAll);
  original.SetFetchRequestMode(network::mojom::FetchRequestMode::kCORS);
  original.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kSameOrigin);
  original.SetRequestorID(30);
  original.SetPluginChildID(40);
  original.SetAppCacheHostID(50);
  original.SetRequestContext(WebURLRequest::kRequestContextAudio);
  original.SetFrameType(WebURLRequest::kFrameTypeNested);
  original.SetHTTPReferrer(
      Referrer("http://www.example.com/referrer.htm", kReferrerPolicyDefault));

  EXPECT_STREQ("http://www.example.com/test.htm",
               original.Url().GetString().Utf8().data());
  EXPECT_EQ(mojom::FetchCacheMode::kDefault, original.GetCacheMode());
  EXPECT_EQ(10, original.TimeoutInterval());
  EXPECT_STREQ("http://www.example.com/first_party.htm",
               original.SiteForCookies().GetString().Utf8().data());
  EXPECT_STREQ("www.example.com",
               original.RequestorOrigin()->Host().Utf8().data());
  EXPECT_STREQ("GET", original.HttpMethod().Utf8().data());
  EXPECT_STREQ("Bar", original.HttpHeaderFields().Get("Foo").Utf8().data());
  EXPECT_STREQ("Fuga", original.HttpHeaderFields().Get("Piyo").Utf8().data());
  EXPECT_EQ(kResourceLoadPriorityLow, original.Priority());
  EXPECT_STREQ("Test Body",
               original.HttpBody()->FlattenToString().Utf8().data());
  EXPECT_FALSE(original.AllowStoredCredentials());
  EXPECT_FALSE(original.ReportUploadProgress());
  EXPECT_FALSE(original.HasUserGesture());
  EXPECT_FALSE(original.DownloadToFile());
  EXPECT_EQ(WebURLRequest::ServiceWorkerMode::kAll,
            original.GetServiceWorkerMode());
  EXPECT_EQ(network::mojom::FetchRequestMode::kCORS,
            original.GetFetchRequestMode());
  EXPECT_EQ(network::mojom::FetchCredentialsMode::kSameOrigin,
            original.GetFetchCredentialsMode());
  EXPECT_EQ(30, original.RequestorID());
  EXPECT_EQ(40, original.GetPluginChildID());
  EXPECT_EQ(50, original.AppCacheHostID());
  EXPECT_EQ(WebURLRequest::kRequestContextAudio, original.GetRequestContext());
  EXPECT_EQ(WebURLRequest::kFrameTypeNested, original.GetFrameType());
  EXPECT_STREQ("http://www.example.com/referrer.htm",
               original.HttpReferrer().Utf8().data());
  EXPECT_EQ(kReferrerPolicyDefault, original.GetReferrerPolicy());

  std::unique_ptr<CrossThreadResourceRequestData> data1(original.CopyData());
  ResourceRequest copy1(data1.get());

  EXPECT_STREQ("http://www.example.com/test.htm",
               copy1.Url().GetString().Utf8().data());
  EXPECT_EQ(mojom::FetchCacheMode::kDefault, copy1.GetCacheMode());
  EXPECT_EQ(10, copy1.TimeoutInterval());
  EXPECT_STREQ("http://www.example.com/first_party.htm",
               copy1.SiteForCookies().GetString().Utf8().data());
  EXPECT_STREQ("www.example.com",
               copy1.RequestorOrigin()->Host().Utf8().data());
  EXPECT_STREQ("GET", copy1.HttpMethod().Utf8().data());
  EXPECT_STREQ("Bar", copy1.HttpHeaderFields().Get("Foo").Utf8().data());
  EXPECT_EQ(kResourceLoadPriorityLow, copy1.Priority());
  EXPECT_STREQ("Test Body", copy1.HttpBody()->FlattenToString().Utf8().data());
  EXPECT_FALSE(copy1.AllowStoredCredentials());
  EXPECT_FALSE(copy1.ReportUploadProgress());
  EXPECT_FALSE(copy1.HasUserGesture());
  EXPECT_FALSE(copy1.DownloadToFile());
  EXPECT_EQ(WebURLRequest::ServiceWorkerMode::kAll,
            copy1.GetServiceWorkerMode());
  EXPECT_EQ(network::mojom::FetchRequestMode::kCORS,
            copy1.GetFetchRequestMode());
  EXPECT_EQ(network::mojom::FetchCredentialsMode::kSameOrigin,
            copy1.GetFetchCredentialsMode());
  EXPECT_EQ(30, copy1.RequestorID());
  EXPECT_EQ(40, copy1.GetPluginChildID());
  EXPECT_EQ(50, copy1.AppCacheHostID());
  EXPECT_EQ(WebURLRequest::kRequestContextAudio, copy1.GetRequestContext());
  EXPECT_EQ(WebURLRequest::kFrameTypeNested, copy1.GetFrameType());
  EXPECT_STREQ("http://www.example.com/referrer.htm",
               copy1.HttpReferrer().Utf8().data());
  EXPECT_EQ(kReferrerPolicyDefault, copy1.GetReferrerPolicy());

  copy1.SetAllowStoredCredentials(true);
  copy1.SetReportUploadProgress(true);
  copy1.SetHasUserGesture(true);
  copy1.SetDownloadToFile(true);
  copy1.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kNone);
  copy1.SetFetchRequestMode(network::mojom::FetchRequestMode::kNoCORS);
  copy1.SetFetchCredentialsMode(network::mojom::FetchCredentialsMode::kInclude);

  std::unique_ptr<CrossThreadResourceRequestData> data2(copy1.CopyData());
  ResourceRequest copy2(data2.get());
  EXPECT_TRUE(copy2.AllowStoredCredentials());
  EXPECT_TRUE(copy2.ReportUploadProgress());
  EXPECT_TRUE(copy2.HasUserGesture());
  EXPECT_TRUE(copy2.DownloadToFile());
  EXPECT_EQ(WebURLRequest::ServiceWorkerMode::kNone,
            copy2.GetServiceWorkerMode());
  EXPECT_EQ(network::mojom::FetchRequestMode::kNoCORS,
            copy1.GetFetchRequestMode());
  EXPECT_EQ(network::mojom::FetchCredentialsMode::kInclude,
            copy1.GetFetchCredentialsMode());
}

TEST(ResourceRequestTest, SetHasUserGesture) {
  ResourceRequest original;
  EXPECT_FALSE(original.HasUserGesture());
  original.SetHasUserGesture(true);
  EXPECT_TRUE(original.HasUserGesture());
  original.SetHasUserGesture(false);
  EXPECT_TRUE(original.HasUserGesture());
}

}  // namespace blink
