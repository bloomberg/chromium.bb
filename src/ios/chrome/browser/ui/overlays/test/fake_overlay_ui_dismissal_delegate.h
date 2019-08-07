// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_UI_DISMISSAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_UI_DISMISSAL_DELEGATE_H_

#import "ios/chrome/browser/ui/overlays/overlay_ui_dismissal_delegate.h"

#include <set>

// Fake implementation of OverlayUIDismissalDelegate.
class FakeDismissalDelegate : public OverlayUIDismissalDelegate {
 public:
  FakeDismissalDelegate();
  ~FakeDismissalDelegate() override;

  // Whether the overlay UI for |request| has been dismissed.
  bool HasUIBeenDismissed(OverlayRequest* request) const;

  // OverlayUIDismissalDelegate:
  void OverlayUIDidFinishDismissal(OverlayRequest* request) override;

 private:
  std::set<OverlayRequest*> dismissed_requests_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_TEST_FAKE_OVERLAY_UI_DISMISSAL_DELEGATE_H_
