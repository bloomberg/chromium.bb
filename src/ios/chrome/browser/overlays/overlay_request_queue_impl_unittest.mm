// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include "ios/chrome/browser/overlays/overlay_request.h"
#include "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"
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
    : public OverlayRequestQueueImplObserver {
 public:
  MockOverlayRequestQueueImplObserver() {}
  ~MockOverlayRequestQueueImplObserver() {}

  MOCK_METHOD2(OnRequestAdded, void(OverlayRequestQueue*, OverlayRequest*));
  MOCK_METHOD2(OnRequestRemoved, void(OverlayRequestQueue*, OverlayRequest*));
};
}  // namespace

// Test fixture for RequestQueueImpl.
class OverlayRequestQueueImplTest : public PlatformTest {
 public:
  OverlayRequestQueueImplTest() : PlatformTest() {
    OverlayRequestQueueImpl::CreateForWebState(&web_state_);
    queue_impl()->AddObserver(&observer_);
  }
  ~OverlayRequestQueueImplTest() override {
    queue_impl()->RemoveObserver(&observer_);
  }

  OverlayRequestQueueImpl* queue_impl() {
    return OverlayRequestQueueImpl::FromWebState(&web_state_);
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_);
  }
  MockOverlayRequestQueueImplObserver& observer() { return observer_; }

 private:
  web::TestWebState web_state_;
  MockOverlayRequestQueueImplObserver observer_;
};

// Tests that Requests can be added and popped from the queue.
TEST_F(OverlayRequestQueueImplTest, AddAndPopRequest) {
  ASSERT_FALSE(queue()->front_request());
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* request_ptr = request.get();
  // Add the request and verify that it's exposed by the queue and received by
  // the observer.
  EXPECT_CALL(observer(), OnRequestAdded(queue(), request_ptr));
  queue_impl()->AddRequest(std::move(request));
  EXPECT_EQ(queue()->front_request(), request_ptr);
  // Remove the request and verify that it's no longer in the queue and the
  // observer callback has been executed.
  EXPECT_CALL(observer(), OnRequestRemoved(queue(), request_ptr));
  queue_impl()->PopRequest();
  ASSERT_FALSE(queue()->front_request());
}
