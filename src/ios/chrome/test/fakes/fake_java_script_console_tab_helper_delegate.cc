// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/fakes/fake_java_script_console_tab_helper_delegate.h"

FakeJavaScriptConsoleTabHelperDelegate::
    FakeJavaScriptConsoleTabHelperDelegate() {}

FakeJavaScriptConsoleTabHelperDelegate::
    ~FakeJavaScriptConsoleTabHelperDelegate() = default;

void FakeJavaScriptConsoleTabHelperDelegate::DidReceiveConsoleMessage(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const JavaScriptConsoleMessage& message) {
  last_received_message_ = std::make_unique<JavaScriptConsoleMessage>(message);
}

const JavaScriptConsoleMessage*
FakeJavaScriptConsoleTabHelperDelegate::GetLastLoggedMessage() const {
  return last_received_message_.get();
}
