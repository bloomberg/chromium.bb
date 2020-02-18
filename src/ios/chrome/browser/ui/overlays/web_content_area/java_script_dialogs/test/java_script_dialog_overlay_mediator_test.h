// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_TEST_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_TEST_H_

#include <memory>

#include "testing/platform_test.h"

@class FakeAlertConsumer;
@class JavaScriptDialogOverlayMediator;

// Test fixture for testing JavaScriptDialogOverlayMediator subclasses.
class JavaScriptDialogOverlayMediatorTest : public PlatformTest {
 protected:
  JavaScriptDialogOverlayMediatorTest();
  ~JavaScriptDialogOverlayMediatorTest() override;

  // Accessors:
  FakeAlertConsumer* consumer() { return consumer_; }

  // Setter for the mediator.  Upon being set, |consumer_| will be updated with
  // the new value.
  void SetMediator(JavaScriptDialogOverlayMediator* mediator);

 private:
  FakeAlertConsumer* consumer_ = nil;
  JavaScriptDialogOverlayMediator* mediator_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_TEST_JAVA_SCRIPT_DIALOG_OVERLAY_MEDIATOR_TEST_H_
