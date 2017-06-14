// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/sim/SimWebFrameClient.h"

#include "core/testing/sim/SimTest.h"
#include "public/web/WebConsoleMessage.h"

namespace blink {

SimWebFrameClient::SimWebFrameClient(SimTest& test) : test_(&test) {}

void SimWebFrameClient::DidAddMessageToConsole(const WebConsoleMessage& message,
                                               const WebString& source_name,
                                               unsigned source_line,
                                               const WebString& stack_trace) {
  test_->AddConsoleMessage(message.text);
}

}  // namespace blink
