// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimWebFrameClient_h
#define SimWebFrameClient_h

#include "core/frame/FrameTestHelpers.h"

namespace blink {

class SimTest;

class SimWebFrameClient final : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit SimWebFrameClient(SimTest&);

  // WebFrameClient overrides:
  void DidAddMessageToConsole(const WebConsoleMessage&,
                              const WebString& source_name,
                              unsigned source_line,
                              const WebString& stack_trace) override;

 private:
  SimTest* test_;
};

}  // namespace blink

#endif
