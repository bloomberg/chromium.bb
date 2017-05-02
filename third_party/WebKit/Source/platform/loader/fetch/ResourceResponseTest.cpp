// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ResourceResponse.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

ResourceResponse CreateTestResponse() {
  ResourceResponse response;
  response.AddHTTPHeaderField("age", "0");
  response.AddHTTPHeaderField("cache-control", "no-cache");
  response.AddHTTPHeaderField("date", "Tue, 17 Jan 2017 04:01:00 GMT");
  response.AddHTTPHeaderField("expires", "Tue, 17 Jan 2017 04:11:00 GMT");
  response.AddHTTPHeaderField("last-modified", "Tue, 17 Jan 2017 04:00:00 GMT");
  response.AddHTTPHeaderField("pragma", "public");
  response.AddHTTPHeaderField("etag", "abc");
  response.AddHTTPHeaderField("content-disposition",
                              "attachment; filename=a.txt");
  return response;
}

void RunHeaderRelatedTest(const ResourceResponse& response) {
  EXPECT_EQ(0, response.Age());
  EXPECT_NE(0, response.Date());
  EXPECT_NE(0, response.Expires());
  EXPECT_NE(0, response.LastModified());
  EXPECT_EQ(true, response.CacheControlContainsNoCache());
}

void RunInThread() {
  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
}

}  // namespace

TEST(ResourceResponseTest, SignedCertificateTimestampIsolatedCopy) {
  ResourceResponse::SignedCertificateTimestamp src(
      "status", "origin", "logDescription", "logId", 7, "hashAlgorithm",
      "signatureAlgorithm", "signatureData");

  ResourceResponse::SignedCertificateTimestamp dest = src.IsolatedCopy();

  EXPECT_EQ(src.status_, dest.status_);
  EXPECT_NE(src.status_.Impl(), dest.status_.Impl());
  EXPECT_EQ(src.origin_, dest.origin_);
  EXPECT_NE(src.origin_.Impl(), dest.origin_.Impl());
  EXPECT_EQ(src.log_description_, dest.log_description_);
  EXPECT_NE(src.log_description_.Impl(), dest.log_description_.Impl());
  EXPECT_EQ(src.log_id_, dest.log_id_);
  EXPECT_NE(src.log_id_.Impl(), dest.log_id_.Impl());
  EXPECT_EQ(src.timestamp_, dest.timestamp_);
  EXPECT_EQ(src.hash_algorithm_, dest.hash_algorithm_);
  EXPECT_NE(src.hash_algorithm_.Impl(), dest.hash_algorithm_.Impl());
  EXPECT_EQ(src.signature_algorithm_, dest.signature_algorithm_);
  EXPECT_NE(src.signature_algorithm_.Impl(), dest.signature_algorithm_.Impl());
  EXPECT_EQ(src.signature_data_, dest.signature_data_);
  EXPECT_NE(src.signature_data_.Impl(), dest.signature_data_.Impl());
}

TEST(ResourceResponseTest, CrossThreadAtomicStrings) {
  // This test checks that AtomicStrings in ResourceResponse doesn't cause the
  // failure of ThreadRestrictionVerifier check.
  ResourceResponse response(CreateTestResponse());
  RunHeaderRelatedTest(response);
  std::unique_ptr<WebThread> thread =
      Platform::Current()->CreateThread("WorkerThread");
  thread->GetWebTaskRunner()->PostTask(BLINK_FROM_HERE,
                                       CrossThreadBind(&RunInThread));
  thread.reset();
}

}  // namespace blink
