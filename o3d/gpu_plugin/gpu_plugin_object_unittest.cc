// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/command_buffer_mock.h"
#include "o3d/gpu_plugin/gpu_plugin_object.h"
#include "o3d/gpu_plugin/np_utils/np_browser_mock.h"
#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/system_services/shared_memory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(O3D_IN_CHROME)
#include "webkit/glue/plugins/nphostapi.h"
#else
#include "npupp.h"
#endif

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class MockSystemNPObject : public DefaultNPObject<NPObject> {
 public:
  explicit MockSystemNPObject(NPP npp) {
  }

  MOCK_METHOD1(CreateSharedMemory, NPObjectPointer<NPObject>(int32 size));

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(MockSystemNPObject, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(CreateSharedMemory,
                        NPObjectPointer<NPObject>(int32 size))
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemNPObject);
};

class GPUPluginObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    plugin_object_ = NPCreateObject<GPUPluginObject>(NULL);

    window_object_ = NPCreateObject<DynamicNPObject>(NULL);
    ON_CALL(mock_browser_, GetWindowNPObject(NULL))
      .WillByDefault(Return(window_object_.ToReturned()));

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

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsInitializedCommandBuffer) {
  NPObjectPointer<NPObject> ring_buffer =
      NPCreateObject<StrictMock<MockSharedMemory> >(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(
      GPUPluginObject::kCommandBufferSize))
    .WillOnce(Return(ring_buffer));

  // Intercept creation of command buffer object and return mock.
  NPObjectPointer<MockCommandBuffer> command_buffer =
      NPCreateObject<MockCommandBuffer>(NULL);
  EXPECT_CALL(mock_browser_, CreateObject(NULL,
      NPGetClass<CommandBuffer>()))
    .WillOnce(Return(command_buffer.ToReturned()));

  EXPECT_CALL(*command_buffer.Get(), Initialize(ring_buffer))
    .WillOnce(Return(true));

  EXPECT_CALL(*command_buffer.Get(), SetPutOffsetChangeCallback(NotNull()));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  EXPECT_EQ(command_buffer, plugin_object_->OpenCommandBuffer());

  // Calling OpenCommandBuffer again just returns the existing command buffer.
  EXPECT_EQ(command_buffer, plugin_object_->OpenCommandBuffer());

  EXPECT_CALL(*command_buffer.Get(), SetPutOffsetChangeCallback(NULL));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest,
    OpenCommandBufferReturnsNullIfCannotCreateRingBuffer) {
  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(
      GPUPluginObject::kCommandBufferSize))
    .WillOnce(Return(NPObjectPointer<NPObject>()));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  EXPECT_EQ(NPObjectPointer<NPObject>(), plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsNullIfCannotInitialize) {
  NPObjectPointer<NPObject> ring_buffer =
      NPCreateObject<StrictMock<MockSharedMemory> >(NULL);

  EXPECT_CALL(*system_object_.Get(), CreateSharedMemory(
      GPUPluginObject::kCommandBufferSize))
    .WillOnce(Return(ring_buffer));

  // Intercept creation of command buffer object and return mock.
  NPObjectPointer<MockCommandBuffer> command_buffer =
      NPCreateObject<StrictMock<MockCommandBuffer> >(NULL);
  EXPECT_CALL(mock_browser_, CreateObject(NULL,
      NPGetClass<CommandBuffer>()))
    .WillOnce(Return(command_buffer.ToReturned()));

  EXPECT_CALL(*command_buffer.Get(), Initialize(ring_buffer))
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
