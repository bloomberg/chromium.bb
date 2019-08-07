// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_FAKE_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_FAKE_WEB_STATE_LIST_DELEGATE_H_

#include "base/macros.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"

// Fake WebStateList delegate for tests.
class FakeWebStateListDelegate : public WebStateListDelegate {
 public:
  FakeWebStateListDelegate();
  ~FakeWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
  void WebStateDetached(web::WebState* web_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWebStateListDelegate);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_FAKE_WEB_STATE_LIST_DELEGATE_H_
