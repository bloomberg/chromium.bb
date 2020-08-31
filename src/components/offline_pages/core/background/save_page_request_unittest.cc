// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/save_page_request.h"

#include "components/offline_pages/core/offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const int64_t kRequestId = 42;
const ClientId kClientId("bookmark", "1234");
const bool kUserRequested = true;
const std::string kRequestOrigin = "abc.xyz";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL Url1() {
  return GURL("http://example.com");
}
GURL Url2() {
  return GURL("http://example.com/test");
}

}  // namespace

class SavePageRequestTest : public testing::Test {
 public:
  ~SavePageRequestTest() override;
};

SavePageRequestTest::~SavePageRequestTest() {}

TEST_F(SavePageRequestTest, CreatePendingReqeust) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  request.set_original_url(Url2());
  EXPECT_EQ(kRequestId, request.request_id());
  EXPECT_EQ(Url1(), request.url());
  EXPECT_EQ(kClientId, request.client_id());
  EXPECT_EQ(creation_time, request.creation_time());
  EXPECT_EQ(base::Time(), request.last_attempt_time());
  EXPECT_EQ(0, request.completed_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
  EXPECT_EQ(0, request.started_attempt_count());
  EXPECT_EQ(0, request.completed_attempt_count());
  EXPECT_EQ(Url2(), request.original_url());
  EXPECT_EQ("", request.request_origin());
}

TEST_F(SavePageRequestTest, StartAndCompleteRequest) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);
  request.set_request_origin(kRequestOrigin);

  base::Time start_time = creation_time + base::TimeDelta::FromHours(3);
  request.MarkAttemptStarted(start_time);

  // Most things don't change about the request.
  EXPECT_EQ(kRequestId, request.request_id());
  EXPECT_EQ(Url1(), request.url());
  EXPECT_EQ(kClientId, request.client_id());
  EXPECT_EQ(creation_time, request.creation_time());
  EXPECT_EQ(kRequestOrigin, request.request_origin());

  // Attempt time, attempt count and status will though.
  EXPECT_EQ(start_time, request.last_attempt_time());
  EXPECT_EQ(1, request.started_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, request.request_state());

  request.MarkAttemptCompleted(FailState::CANNOT_DOWNLOAD);

  // Again, most things don't change about the request.
  EXPECT_EQ(kRequestId, request.request_id());
  EXPECT_EQ(Url1(), request.url());
  EXPECT_EQ(kClientId, request.client_id());
  EXPECT_EQ(creation_time, request.creation_time());

  // Last attempt time and status are updated.
  EXPECT_EQ(1, request.completed_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
}

TEST_F(SavePageRequestTest, StartAndAbortRequest) {
  base::Time creation_time = OfflineTimeNow();
  SavePageRequest request(kRequestId, Url1(), kClientId, creation_time,
                          kUserRequested);

  base::Time start_time = creation_time + base::TimeDelta::FromHours(3);
  request.MarkAttemptStarted(start_time);

  // Most things don't change about the request.
  EXPECT_EQ(kRequestId, request.request_id());
  EXPECT_EQ(Url1(), request.url());
  EXPECT_EQ(kClientId, request.client_id());
  EXPECT_EQ(creation_time, request.creation_time());
  EXPECT_EQ("", request.request_origin());

  // Attempt time and attempt count will though.
  EXPECT_EQ(start_time, request.last_attempt_time());
  EXPECT_EQ(1, request.started_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, request.request_state());

  request.MarkAttemptAborted();

  // Again, most things don't change about the request.
  EXPECT_EQ(kRequestId, request.request_id());
  EXPECT_EQ(Url1(), request.url());
  EXPECT_EQ(kClientId, request.client_id());
  EXPECT_EQ(creation_time, request.creation_time());
  EXPECT_EQ("", request.request_origin());

  // Last attempt time is updated and completed attempt count did not rise.
  EXPECT_EQ(0, request.completed_attempt_count());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, request.request_state());
}

}  // namespace offline_pages
