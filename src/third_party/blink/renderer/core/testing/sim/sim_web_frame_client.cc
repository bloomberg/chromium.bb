// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_web_frame_client.h"

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

SimWebFrameClient::SimWebFrameClient(SimTest& test)
    : test_(&test),
      effective_connection_type_(WebEffectiveConnectionType::kTypeUnknown) {}

void SimWebFrameClient::DidAddMessageToConsole(const WebConsoleMessage& message,
                                               const WebString& source_name,
                                               unsigned source_line,
                                               const WebString& stack_trace) {
  test_->AddConsoleMessage(message.text);
}

WebEffectiveConnectionType SimWebFrameClient::GetEffectiveConnectionType() {
  return effective_connection_type_;
}

void SimWebFrameClient::SetEffectiveConnectionTypeForTesting(
    WebEffectiveConnectionType effective_connection_type) {
  effective_connection_type_ = effective_connection_type;
}

}  // namespace blink
