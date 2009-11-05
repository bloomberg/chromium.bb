// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_MOCK_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_MOCK_H_

#include "o3d/gpu_plugin/np_utils/np_plugin_object.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu_plugin {

class MockPluginObject : public PluginObject {
 public:
  MOCK_METHOD5(New, NPError(NPMIMEType, int16, char*[], char*[], NPSavedData*));
  MOCK_METHOD1(SetWindow, NPError(NPWindow*));
  MOCK_METHOD1(HandleEvent, int16(NPEvent*));
  MOCK_METHOD1(Destroy, NPError(NPSavedData**));
  MOCK_METHOD0(Release, void());
  MOCK_METHOD0(GetScriptableNPObject, NPObject*());
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_PLUGIN_OBJECT_MOCK_H_
