// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/plugin_object_factory.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace o3d {
namespace gpu_plugin {

class PluginObjectFactoryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitializeNPNTestStub();
    factory_ = new PluginObjectFactory;
  }

  virtual void TearDown() {
    delete factory_;
    ShutdownNPNTestStub();
  }

  PluginObjectFactory* factory_;
};

TEST_F(PluginObjectFactoryTest, ReturnsNullForUnknownMimeType) {
  PluginObject* plugin_object = factory_->CreatePluginObject(
      NULL, "application/unknown");
  EXPECT_TRUE(NULL == plugin_object);
}

TEST_F(PluginObjectFactoryTest, CreatesGPUPlugin) {
  PluginObject* plugin_object = factory_->CreatePluginObject(
      NULL, const_cast<NPMIMEType>(GPUPluginObject::kPluginType));
  EXPECT_TRUE(NULL != plugin_object);
}

}  // namespace gpu_plugin
}  // namespace o3d
