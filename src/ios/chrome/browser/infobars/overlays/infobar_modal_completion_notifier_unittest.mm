// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_modal_completion_notifier.h"

#include "base/scoped_observer.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_request_cancel_handler.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Mock InfobarModalCompletionNotifier::Observer for tests.
class MockInfobarModalCompletionNotifierObserver
    : public InfobarModalCompletionNotifier::Observer {
 public:
  MockInfobarModalCompletionNotifierObserver() = default;
  ~MockInfobarModalCompletionNotifierObserver() override = default;

  MOCK_METHOD2(InfobarModalsCompleted,
               void(InfobarModalCompletionNotifier* notifier,
                    InfoBarIOS* infobar));
};
}  // namespace

// Test fixture for InfobarModalCompletionNotifier.
class InfobarModalCompletionNotifierTest : public PlatformTest {
 public:
  InfobarModalCompletionNotifierTest()
      : notifier_(&web_state_), scoped_observer_(&observer_) {
    scoped_observer_.Add(&notifier_);
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  web::TestWebState web_state_;
  FakeInfobarIOS infobar_;
  InfobarModalCompletionNotifier notifier_;
  MockInfobarModalCompletionNotifierObserver observer_;
  ScopedObserver<InfobarModalCompletionNotifier,
                 InfobarModalCompletionNotifier::Observer>
      scoped_observer_;
};

// Tests that the observer is notified when all modal requests for |infobar_|
// have been removed.
TEST_F(InfobarModalCompletionNotifierTest, ModalCompletion) {
  // Add a detail sheet request for |infobar_|.
  std::unique_ptr<OverlayRequest> detail_sheet_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar_, InfobarOverlayType::kDetailSheet);
  std::unique_ptr<FakeOverlayRequestCancelHandler>
      passed_detail_sheet_cancel_handler =
          std::make_unique<FakeOverlayRequestCancelHandler>(
              detail_sheet_request.get(), queue());
  FakeOverlayRequestCancelHandler* detail_sheet_cancel_handler =
      passed_detail_sheet_cancel_handler.get();
  queue()->AddRequest(std::move(detail_sheet_request),
                      std::move(passed_detail_sheet_cancel_handler));
  // Add a modal request for |infobar_|.
  std::unique_ptr<OverlayRequest> modal_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar_, InfobarOverlayType::kModal);
  std::unique_ptr<FakeOverlayRequestCancelHandler> passed_modal_cancel_handler =
      std::make_unique<FakeOverlayRequestCancelHandler>(modal_request.get(),
                                                        queue());
  FakeOverlayRequestCancelHandler* modal_cancel_handler =
      passed_modal_cancel_handler.get();
  queue()->AddRequest(std::move(modal_request),
                      std::move(passed_modal_cancel_handler));

  // Cancel the detail sheet request, resulting in only a single active request
  // for |infobar_| in the queue.
  detail_sheet_cancel_handler->TriggerCancellation();

  // Cancel the modal request, expecting that InfobarModalsCompleted() is
  // called.
  EXPECT_CALL(observer_, InfobarModalsCompleted(&notifier_, &infobar_));
  modal_cancel_handler->TriggerCancellation();
}
