// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/command_buffer_mock.h"
#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/base_np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_browser_mock.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/plugins/nphostapi.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class GPUPluginObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    plugin_object_ = NPCreateObject<GPUPluginObject>(NULL);
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<GPUPluginObject> plugin_object_;
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
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
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

TEST_F(GPUPluginObjectTest, WindowIsNullBeforeSetWindowCalled) {
  NPWindow window = plugin_object_->GetWindow();
  EXPECT_EQ(NULL, window.window);
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

TEST_F(GPUPluginObjectTest, CanGetScriptableNPObject) {
  EXPECT_EQ(plugin_object_.Get(), plugin_object_->GetScriptableNPObject());
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsInitializedCommandBuffer) {
  // Intercept creation of command buffer object and return mock.
  NPObjectPointer<MockCommandBuffer> command_buffer_object =
      NPCreateObject<StrictMock<MockCommandBuffer> >(NULL);
  EXPECT_CALL(mock_browser_, CreateObject(NULL,
      BaseNPObject::GetNPClass<CommandBuffer>()))
    .WillOnce(Return(command_buffer_object.ToReturned()));

  EXPECT_CALL(*command_buffer_object.Get(), Initialize(1024))
    .WillOnce(Return(true));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  EXPECT_EQ(command_buffer_object, plugin_object_->OpenCommandBuffer());

  // Calling OpenCommandBuffer again just returns the existing command buffer.
  EXPECT_EQ(command_buffer_object, plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsNullIfCannotInitialize) {
  // Intercept creation of command buffer object and return mock.
  NPObjectPointer<MockCommandBuffer> command_buffer_object =
      NPCreateObject<StrictMock<MockCommandBuffer> >(NULL);
  EXPECT_CALL(mock_browser_, CreateObject(NULL,
      BaseNPObject::GetNPClass<CommandBuffer>()))
    .WillOnce(Return(command_buffer_object.ToReturned()));

  EXPECT_CALL(*command_buffer_object.Get(), Initialize(1024))
    .WillOnce(Return(false));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  EXPECT_EQ(NPObjectPointer<NPObject>(), plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}
}  // namespace gpu_plugin
}  // namespace o3d
