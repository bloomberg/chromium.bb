// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebPresentationClient_h
#define MockWebPresentationClient_h

#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockWebPresentationClient : public WebPresentationClient {
 public:
  MOCK_METHOD1(SetReceiver, void(WebPresentationReceiver*));
};

}  // namespace blink

#endif  // MockWebPresentationClient_h
