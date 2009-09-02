// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_MOCK_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_MOCK_H_

#include "o3d/gpu_plugin/np_utils/np_browser_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace o3d {
namespace gpu_plugin {

// This mocks certain member functions of the stub browser. Those relating
// to identifiers, memory management, reference counting and forwarding to
// NPObjects are deliberately not mocked so the mock browser can be used as
// normal for these calls.
class MockNPBrowser : public StubNPBrowser {
 public:
  MOCK_METHOD1(GetWindowNPObject, NPObject*(NPP));
  MOCK_METHOD4(MapSharedMemory, NPSharedMemory*(NPP, NPObject*, size_t, bool));
  MOCK_METHOD2(UnmapSharedMemory, void(NPP, NPSharedMemory*));
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_MOCK_H_
