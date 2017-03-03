// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimWebFrameClient_h
#define SimWebFrameClient_h

#include "web/tests/FrameTestHelpers.h"

namespace blink {

class SimTest;

class SimWebFrameClient final : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit SimWebFrameClient(SimTest&);

  // WebFrameClient overrides:
  void didAddMessageToConsole(const WebConsoleMessage&,
                              const WebString& sourceName,
                              unsigned sourceLine,
                              const WebString& stackTrace) override;

 private:
  SimTest* m_test;
};

}  // namespace blink

#endif
