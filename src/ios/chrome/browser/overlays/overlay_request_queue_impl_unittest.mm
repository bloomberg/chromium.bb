// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Mock queue observer.
class MockOverlayRequestQueueImplObserver
    : public OverlayRequestQueueImpl::Observer {
 public:
  MockOverlayRequestQueueImplObserver() {}
  ~MockOverlayRequestQueueImplObserver() {}

  MOCK_METHOD2(RequestAddedToQueue,
               void(OverlayRequestQueueImpl*, OverlayRequest*));
  MOCK_METHOD2(QueuedRequestCancelled,
               void(OverlayRequestQueueImpl*, OverlayRequest*));
};
}  // namespace

// Test fixture for RequestQueueImpl.
class OverlayRequestQueueImplTest : public PlatformTest {
 public:
  OverlayRequestQueueImplTest() : PlatformTest() {
    OverlayRequestQueueImpl::Container::CreateForWebState(&web_state_);
    queue()->AddObserver(&observer_);
  }
  ~OverlayRequestQueueImplTest() override {
    queue()->RemoveObserver(&observer_);
  }

  OverlayRequestQueueImpl* queue() {
    // Use the kWebContentArea queue for testing.
    return OverlayRequestQueueImpl::Container::FromWebState(&web_state_)
        ->QueueForModality(OverlayModality::kWebContentArea);
  }
  MockOverlayRequestQueueImplObserver& observer() { return observer_; }

  OverlayRequest* AddRequest() {
    std::unique_ptr<OverlayRequest> passed_request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
    OverlayRequest* request = passed_request.get();
    EXPECT_CALL(observer(), RequestAddedToQueue(queue(), request));
    queue()->AddRequest(std::move(passed_request));
    return request;
  }

 private:
  web::TestWebState web_state_;
  MockOverlayRequestQueueImplObserver observer_;
};

// Tests that state is updated correctly and observer callbacks are received
// when adding requests to the back of the queue.
TEST_F(OverlayRequestQueueImplTest, AddRequest) {
  OverlayRequest* first_request = AddRequest();
  AddRequest();

  EXPECT_EQ(first_request, queue()->front_request());
  EXPECT_EQ(2U, queue()->size());
}

// Tests that state is updated correctly and observer callbacks are received
// when popping the requests.
TEST_F(OverlayRequestQueueImplTest, PopRequests) {
  // Add three requests to the queue.
  OverlayRequest* first_request = AddRequest();
  OverlayRequest* second_request = AddRequest();
  AddRequest();

  ASSERT_EQ(first_request, queue()->front_request());
  ASSERT_EQ(3U, queue()->size());

  // Pop the first request and check that the size and front request have been
  // updated.
  queue()->PopFrontRequest();
  EXPECT_EQ(second_request, queue()->front_request());
  EXPECT_EQ(2U, queue()->size());

  // Pop the third request and check that the second request is still frontmost
  // and that the size is updated.
  queue()->PopBackRequest();
  EXPECT_EQ(second_request, queue()->front_request());
  EXPECT_EQ(1U, queue()->size());
}

// Tests that state is updated correctly and observer callbacks are received
// when popping the requests.
TEST_F(OverlayRequestQueueImplTest, CancelAllRequests) {
  // Add two requests to the queue then cancel all requests, verifying that
  // the observer callback is received for each.
  OverlayRequest* first_request = AddRequest();
  OverlayRequest* second_request = AddRequest();

  EXPECT_CALL(observer(), QueuedRequestCancelled(queue(), first_request));
  EXPECT_CALL(observer(), QueuedRequestCancelled(queue(), second_request));
  queue()->CancelAllRequests();

  EXPECT_EQ(0U, queue()->size());
  EXPECT_TRUE(queue()->empty());
}
