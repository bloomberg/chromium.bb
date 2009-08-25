// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace o3d {
namespace gpu_plugin {

class GPUPluginObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitializeNPNTestStub();

    np_class_ = const_cast<NPClass*>(
        BaseNPObject::GetNPClass<GPUPluginObject>());

    plugin_object_ = static_cast<GPUPluginObject*>(
            gpu_plugin::NPN_CreateObject(NULL, np_class_));
  }

  virtual void TearDown() {
    gpu_plugin::NPN_ReleaseObject(plugin_object_);
    ShutdownNPNTestStub();
  }

  NPClass* np_class_;
  GPUPluginObject* plugin_object_;
};

TEST_F(GPUPluginObjectTest, CanInitializeAndDestroyPluginObject) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, DestroyFailsIfNotInitialized) {
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, NewFailsIfAlreadyInitialized) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));
}

TEST_F(GPUPluginObjectTest, NewFailsIfObjectHasPreviouslyBeenDestroyed) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_object_->New("application/foo",
                                                     0,
                                                     NULL,
                                                     NULL,
                                                     NULL));
}

TEST_F(GPUPluginObjectTest, CanSetWindow) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPWindow window = {0};
  window.window = &window;
  window.x = 7;

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->SetWindow(&window));
  EXPECT_EQ(0, memcmp(&window, &plugin_object_->GetWindow(), sizeof(window)));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, SetWindowFailsIfNotInitialized) {
  NPWindow window = {0};
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_object_->SetWindow(&window));
}

}  // namespace gpu_plugin
}  // namespace o3d
