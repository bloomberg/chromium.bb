// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceResponse.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

ResourceResponse createTestResponse() {
  ResourceResponse response;
  response.addHTTPHeaderField("age", "0");
  response.addHTTPHeaderField("cache-control", "no-cache");
  response.addHTTPHeaderField("date", "Tue, 17 Jan 2017 04:01:00 GMT");
  response.addHTTPHeaderField("expires", "Tue, 17 Jan 2017 04:11:00 GMT");
  response.addHTTPHeaderField("last-modified", "Tue, 17 Jan 2017 04:00:00 GMT");
  response.addHTTPHeaderField("pragma", "public");
  response.addHTTPHeaderField("etag", "abc");
  response.addHTTPHeaderField("content-disposition",
                              "attachment; filename=a.txt");
  return response;
}

void runHeaderRelatedTest(const ResourceResponse& response) {
  EXPECT_EQ(0, response.age());
  EXPECT_NE(0, response.date());
  EXPECT_NE(0, response.expires());
  EXPECT_NE(0, response.lastModified());
  EXPECT_EQ(true, response.cacheControlContainsNoCache());
}

void runInThread() {
  ResourceResponse response(createTestResponse());
  runHeaderRelatedTest(response);
}

}  // namespace

TEST(ResourceResponseTest, SignedCertificateTimestampIsolatedCopy) {
  ResourceResponse::SignedCertificateTimestamp src(
      "status", "origin", "logDescription", "logId", 7, "hashAlgorithm",
      "signatureAlgorithm", "signatureData");

  ResourceResponse::SignedCertificateTimestamp dest = src.isolatedCopy();

  EXPECT_EQ(src.m_status, dest.m_status);
  EXPECT_NE(src.m_status.impl(), dest.m_status.impl());
  EXPECT_EQ(src.m_origin, dest.m_origin);
  EXPECT_NE(src.m_origin.impl(), dest.m_origin.impl());
  EXPECT_EQ(src.m_logDescription, dest.m_logDescription);
  EXPECT_NE(src.m_logDescription.impl(), dest.m_logDescription.impl());
  EXPECT_EQ(src.m_logId, dest.m_logId);
  EXPECT_NE(src.m_logId.impl(), dest.m_logId.impl());
  EXPECT_EQ(src.m_timestamp, dest.m_timestamp);
  EXPECT_EQ(src.m_hashAlgorithm, dest.m_hashAlgorithm);
  EXPECT_NE(src.m_hashAlgorithm.impl(), dest.m_hashAlgorithm.impl());
  EXPECT_EQ(src.m_signatureAlgorithm, dest.m_signatureAlgorithm);
  EXPECT_NE(src.m_signatureAlgorithm.impl(), dest.m_signatureAlgorithm.impl());
  EXPECT_EQ(src.m_signatureData, dest.m_signatureData);
  EXPECT_NE(src.m_signatureData.impl(), dest.m_signatureData.impl());
}

TEST(ResourceResponseTest, CrossThreadAtomicStrings) {
  // This test checks that AtomicStrings in ResourceResponse doesn't cause the
  // failure of ThreadRestrictionVerifier check.
  ResourceResponse response(createTestResponse());
  runHeaderRelatedTest(response);
  std::unique_ptr<WebThread> thread =
      WTF::wrapUnique(Platform::current()->createThread("WorkerThread"));
  thread->getWebTaskRunner()->postTask(BLINK_FROM_HERE,
                                       crossThreadBind(&runInThread));
  thread.reset();
}

}  // namespace blink
