// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_MOCK_H_
#define O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_MOCK_H_

#include "o3d/gpu_plugin/plugin_object_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace o3d {
namespace gpu_plugin {

// Mockable factory used to create instances of PluginObject based on plugin
// mime type.
class MockPluginObjectFactory : public PluginObjectFactory {
 public:
  MOCK_METHOD2(CreatePluginObject, PluginObject*(NPP, NPMIMEType));
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_PLUGIN_OBJECT_FACTORY_MOCK_H_
