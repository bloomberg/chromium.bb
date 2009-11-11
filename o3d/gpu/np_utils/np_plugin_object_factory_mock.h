// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_MOCK_H_
#define GPU_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_MOCK_H_

#include "gpu/np_utils/np_plugin_object_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu_plugin {

// Mockable factory used to create instances of PluginObject based on plugin
// mime type.
class MockPluginObjectFactory : public NPPluginObjectFactory {
 public:
  MOCK_METHOD2(CreatePluginObject, PluginObject*(NPP, NPMIMEType));
};

}  // namespace gpu_plugin

#endif  // GPU_NP_UTILS_NP_PLUGIN_OBJECT_FACTORY_MOCK_H_
