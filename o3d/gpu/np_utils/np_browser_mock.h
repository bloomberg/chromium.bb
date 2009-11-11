// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_NP_UTILS_NP_BROWSER_MOCK_H_
#define GPU_NP_UTILS_NP_BROWSER_MOCK_H_

#include "gpu/np_utils/np_browser_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu_plugin {

// This mocks certain member functions of the stub browser. Those relating
// to identifiers, memory management, reference counting and forwarding to
// NPObjects are deliberately not mocked so the mock browser can be used as
// normal for these calls.
class MockNPBrowser : public StubNPBrowser {
 public:
  NPObject* ConcreteCreateObject(NPP npp, const NPClass* cl) {
    return StubNPBrowser::CreateObject(npp, cl);
  }

  MockNPBrowser() {
    // Do not mock CreateObject by default but allow it to be mocked so object
    // creation can be intercepted.
    ON_CALL(*this, CreateObject(testing::_, testing::_))
      .WillByDefault(testing::Invoke(this,
                                     &MockNPBrowser::ConcreteCreateObject));
  }

  void ConcretePluginThreadAsyncCall(NPP npp,
                                     PluginThreadAsyncCallProc callback,
                                     void* data) {
    return StubNPBrowser::PluginThreadAsyncCall(npp, callback, data);
  }

  MOCK_METHOD2(CreateObject, NPObject*(NPP npp, const NPClass* cl));
  MOCK_METHOD1(GetWindowNPObject, NPObject*(NPP cpp));
  MOCK_METHOD3(PluginThreadAsyncCall,
               void(NPP npp, PluginThreadAsyncCallProc callback, void* data));
  MOCK_METHOD4(ScheduleTimer, uint32(NPP npp,
                                     uint32 interval,
                                     bool repeat,
                                     TimerProc callback));
  MOCK_METHOD2(UnscheduleTimer, void(NPP npp, uint32 timer_id));
};

}  // namespace gpu_plugin

#endif  // GPU_NP_UTILS_NP_BROWSER_MOCK_H_
