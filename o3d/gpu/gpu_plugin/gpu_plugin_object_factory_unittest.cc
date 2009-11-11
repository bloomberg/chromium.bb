// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gpu_plugin/gpu_plugin_object.h"
#include "gpu/gpu_plugin/gpu_plugin_object_factory.h"
#include "gpu/np_utils/np_browser_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu_plugin {

class PluginObjectFactoryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    factory_ = new GPUPluginObjectFactory;
  }

  virtual void TearDown() {
    delete factory_;
  }

  StubNPBrowser stub_browser_;
  GPUPluginObjectFactory* factory_;
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
