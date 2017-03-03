// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimWebFrameClient.h"

#include "public/web/WebConsoleMessage.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

SimWebFrameClient::SimWebFrameClient(SimTest& test) : m_test(&test) {}

void SimWebFrameClient::didAddMessageToConsole(const WebConsoleMessage& message,
                                               const WebString& sourceName,
                                               unsigned sourceLine,
                                               const WebString& stackTrace) {
  m_test->addConsoleMessage(message.text);
}

}  // namespace blink
