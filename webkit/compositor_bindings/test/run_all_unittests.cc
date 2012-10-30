// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_suite.h"
#include "cc/proxy.h"
#include "cc/thread_impl.h"
#include <gmock/gmock.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite testSuite(argc, argv);
  MessageLoop message_loop;
  scoped_ptr<cc::Thread> mainCCThread = cc::ThreadImpl::createForCurrentThread();
  cc::Proxy::setMainThread(mainCCThread.get());
  int result = testSuite.Run();

  return result;
}

