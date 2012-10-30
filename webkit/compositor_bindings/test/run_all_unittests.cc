// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_suite.h"
#include "cc/test/test_webkit_platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include <gmock/gmock.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite testSuite(argc, argv);
  cc::TestWebKitPlatform platform;
  MessageLoop message_loop;
  WebKit::Platform::initialize(&platform);
  int result = testSuite.Run();

  return result;
}

