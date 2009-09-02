// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/base_np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"
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

class MockSystemNPObject : public DispatchedNPObject {
 public:
  explicit MockSystemNPObject(NPP npp) : DispatchedNPObject(npp) {
  }

  MOCK_METHOD1(CreateSharedMemory, NPObjectPointer<NPObject>(int32));

 protected:
  NP_UTILS_BEGIN_DISPATCHER_CHAIN(MockSystemNPObject, DispatchedNPObject)
    NP_UTILS_DISPATCHER(CreateSharedMemory, NPObjectPointer<NPObject>(int32))
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemNPObject);
};

class GPUPluginObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    plugin_object_ = NPCreateObject<GPUPluginObject>(NULL);

    window_object_ = NPCreateObject<DynamicNPObject>(NULL);

    chromium_object_ = NPCreateObject<DynamicNPObject>(NULL);
    NPSetProperty(NULL, window_object_, "chromium", chromium_object_);

    system_object_ = NPCreateObject<StrictMock<MockSystemNPObject> >(NULL);
    NPSetProperty(NULL, chromium_object_, "system", system_object_);
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<GPUPluginObject> plugin_object_;
  NPObjectPointer<DynamicNPObject> window_object_;
  NPObjectPointer<DynamicNPObject> chromium_object_;
  NPObjectPointer<MockSystemNPObject> system_object_;
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

TEST_F(GPUPluginObjectTest, OpenCommandBufferCreatesAndMapsCommandBuffer) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  NPObjectPointer<BaseNPObject> expected_command_buffer =
      NPCreateObject<BaseNPObject>(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(expected_command_buffer));

  NPSharedMemory shared_memory;

  EXPECT_CALL(mock_browser_, MapSharedMemory(NULL,
                                             expected_command_buffer.Get(),
                                             1024,
                                             false))
    .WillOnce(Return(&shared_memory));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPObjectPointer<NPObject> actual_command_buffer =
      plugin_object_->OpenCommandBuffer();

  EXPECT_EQ(expected_command_buffer, actual_command_buffer);

  // Calling OpenCommandBuffer again just returns the existing command buffer.
  actual_command_buffer = plugin_object_->OpenCommandBuffer();
  EXPECT_EQ(expected_command_buffer, actual_command_buffer);

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferFailsIfCannotCreateSharedMemory) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(NPObjectPointer<BaseNPObject>()));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPObjectPointer<NPObject> actual_command_buffer =
      plugin_object_->OpenCommandBuffer();

  EXPECT_EQ(NPObjectPointer<NPObject>(), actual_command_buffer);

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferFailsIfCannotMapSharedMemory) {
  EXPECT_CALL(mock_browser_, GetWindowNPObject(NULL))
    .WillOnce(Return(window_object_.ToReturned()));

  NPObjectPointer<BaseNPObject> expected_command_buffer =
      NPCreateObject<BaseNPObject>(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(1024))
    .WillOnce(Return(expected_command_buffer));

  EXPECT_CALL(mock_browser_, MapSharedMemory(NULL,
                                             expected_command_buffer.Get(),
                                             1024,
                                             false))
    .WillOnce(Return(static_cast<NPSharedMemory*>(NULL)));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPObjectPointer<NPObject> actual_command_buffer =
      plugin_object_->OpenCommandBuffer();

  EXPECT_EQ(NPObjectPointer<NPObject>(), actual_command_buffer);

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}
}  // namespace gpu_plugin
}  // namespace o3d
