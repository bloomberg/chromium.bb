// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/command_buffer_mock.h"
#include "gpu/gpu_plugin/gpu_plugin_object.h"
#include "gpu/command_buffer/service/gpu_processor_mock.h"
#include "gpu/np_utils/np_browser_mock.h"
#include "gpu/np_utils/dynamic_np_object.h"
#include "gpu/np_utils/np_object_mock.h"
#include "gpu/np_utils/np_object_pointer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(O3D_IN_CHROME)
#include "webkit/glue/plugins/nphostapi.h"
#else
#include "o3d/third_party/npapi/include/npfunctions.h"
#endif

using ::base::SharedMemory;
using command_buffer::GPUProcessor;
using command_buffer::MockCommandBuffer;
using command_buffer::MockGPUProcessor;
using np_utils::MockNPBrowser;
using np_utils::NPBrowser;
using np_utils::NPObjectPointer;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace gpu_plugin {

class GPUPluginObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    plugin_object_ = np_utils::NPCreateObject<GPUPluginObject>(NULL);

    command_buffer_ = new MockCommandBuffer;

    // Takes ownership.
    plugin_object_->set_command_buffer(command_buffer_);

    processor_ = new MockGPUProcessor(command_buffer_);
    plugin_object_->set_gpu_processor(processor_.get());
  }

  MockNPBrowser mock_browser_;
  NPObjectPointer<GPUPluginObject> plugin_object_;
  MockCommandBuffer* command_buffer_;
  scoped_refptr<MockGPUProcessor> processor_;
};

namespace {
template <typename T>
void DeleteObject(T* object) {
  delete object;
}
}  // namespace anonymous


TEST_F(GPUPluginObjectTest, CanInstantiateAndDestroyPluginObject) {
  EXPECT_EQ(GPUPluginObject::kWaitingForNew, plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  EXPECT_EQ(GPUPluginObject::kWaitingForSetWindow, plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));

  EXPECT_EQ(GPUPluginObject::kDestroyed, plugin_object_->GetStatus());
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

  EXPECT_EQ(GPUPluginObject::kWaitingForSetWindow, plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));

  EXPECT_EQ(GPUPluginObject::kDestroyed, plugin_object_->GetStatus());
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

  EXPECT_EQ(GPUPluginObject::kDestroyed, plugin_object_->GetStatus());
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
  EXPECT_EQ(GPUPluginObject::kWaitingForOpenCommandBuffer,
            plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, CanGetWindowSize) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPWindow window = {0};
  window.window = &window;
  window.x = 10;
  window.y = 10;
  window.width = 100;
  window.height = 200;

  EXPECT_EQ(0, plugin_object_->GetWidth());
  EXPECT_EQ(0, plugin_object_->GetHeight());
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->SetWindow(&window));
  EXPECT_EQ(100, plugin_object_->GetWidth());
  EXPECT_EQ(200, plugin_object_->GetHeight());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, SetWindowFailsIfNotInitialized) {
  NPWindow window = {0};
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_object_->SetWindow(&window));
  EXPECT_EQ(GPUPluginObject::kWaitingForNew, plugin_object_->GetStatus());
}

TEST_F(GPUPluginObjectTest, CanGetScriptableNPObject) {
  NPObject* scriptable_object = plugin_object_->GetScriptableNPObject();
  EXPECT_EQ(plugin_object_.Get(), scriptable_object);
  NPBrowser::get()->ReleaseObject(scriptable_object);
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsInitializedCommandBuffer) {
  EXPECT_CALL(*command_buffer_, Initialize(NotNull()))
    .WillOnce(DoAll(Invoke(DeleteObject<SharedMemory>),
                    Return(true)));

  EXPECT_CALL(*processor_.get(), Initialize(NULL))
    .WillOnce(Return(true));

  EXPECT_CALL(*command_buffer_, SetPutOffsetChangeCallback(NotNull()))
    .WillOnce(Invoke(DeleteObject<Callback0::Type>));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  // Set status as though SetWindow has been called. Avoids having to create a
  // valid window handle to pass to SetWindow in tests.
  plugin_object_->set_status(GPUPluginObject::kWaitingForOpenCommandBuffer);

  EXPECT_EQ(command_buffer_, plugin_object_->OpenCommandBuffer());

  // Calling OpenCommandBuffer again just returns the existing command buffer.
  EXPECT_EQ(command_buffer_, plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(GPUPluginObject::kInitializationSuccessful,
            plugin_object_->GetStatus());

  EXPECT_CALL(*command_buffer_, SetPutOffsetChangeCallback(NULL));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest, OpenCommandBufferReturnsNullIfWindowNotReady) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  // Set status as though SetWindow has not been called.
  plugin_object_->set_status(GPUPluginObject::kWaitingForSetWindow);

  EXPECT_TRUE(NULL == plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(GPUPluginObject::kWaitingForSetWindow, plugin_object_->GetStatus());
}


TEST_F(GPUPluginObjectTest,
    OpenCommandBufferReturnsNullIfCommandBufferCannotInitialize) {
  EXPECT_CALL(*command_buffer_, Initialize(NotNull()))
    .WillOnce(DoAll(Invoke(DeleteObject<SharedMemory>),
                    Return(false)));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  // Set status as though SetWindow has been called. Avoids having to create a
  // valid window handle to pass to SetWindow in tests.
  plugin_object_->set_status(GPUPluginObject::kWaitingForOpenCommandBuffer);

  EXPECT_TRUE(NULL == plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(GPUPluginObject::kWaitingForOpenCommandBuffer,
            plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

TEST_F(GPUPluginObjectTest,
    OpenCommandBufferReturnsNullIGPUProcessorCannotInitialize) {
  EXPECT_CALL(*command_buffer_, Initialize(NotNull()))
    .WillOnce(DoAll(Invoke(DeleteObject<SharedMemory>),
                    Return(true)));

  EXPECT_CALL(*processor_.get(), Initialize(NULL))
    .WillOnce(Return(false));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  // Set status as though SetWindow has been called. Avoids having to create a
  // valid window handle to pass to SetWindow in tests.
  plugin_object_->set_status(GPUPluginObject::kWaitingForOpenCommandBuffer);

  EXPECT_TRUE(NULL == plugin_object_->OpenCommandBuffer());

  EXPECT_EQ(GPUPluginObject::kWaitingForOpenCommandBuffer,
            plugin_object_->GetStatus());

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

class MockEventSync : public np_utils::DefaultNPObject<NPObject> {
 public:
  explicit MockEventSync(NPP npp) {
  }

  MOCK_METHOD2(Resize, void(int32 width, int32 height));

  NP_UTILS_BEGIN_DISPATCHER_CHAIN(MockEventSync, DefaultNPObject<NPObject>)
    NP_UTILS_DISPATCHER(Resize, void(int32 width, int32 height))
  NP_UTILS_END_DISPATCHER_CHAIN

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventSync);
};

TEST_F(GPUPluginObjectTest, SendsResizeEventOnSetWindow) {
  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->New("application/foo",
                                                0,
                                                NULL,
                                                NULL,
                                                NULL));

  NPObjectPointer<MockEventSync> event_sync =
      np_utils::NPCreateObject<MockEventSync>(NULL);
  plugin_object_->SetEventSync(event_sync);

  EXPECT_CALL(*event_sync.Get(), Resize(100, 200));

  NPWindow window = {0};
  window.window = &window;
  window.x = 10;
  window.y = 10;
  window.width = 100;
  window.height = 200;

  plugin_object_->SetWindow(&window);

  EXPECT_EQ(NPERR_NO_ERROR, plugin_object_->Destroy(NULL));
}

}  // namespace gpu_plugin
