// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::Sequence;
using testing::StrEq;
using testing::StrictMock;

// Mock class to add expectations on the wire calling callbacks
class MockCreateComputePipelineAsyncCallback {
  public:
    MOCK_METHOD(void,
                Call,
                (WGPUCreatePipelineAsyncStatus status,
                 WGPUComputePipeline pipeline,
                 const char* message,
                 void* userdata));
};

std::unique_ptr<StrictMock<MockCreateComputePipelineAsyncCallback>>
    mockCreateComputePipelineAsyncCallback;
void ToMockCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status,
                                              WGPUComputePipeline pipeline,
                                              const char* message,
                                              void* userdata) {
    mockCreateComputePipelineAsyncCallback->Call(status, pipeline, message, userdata);
}

class MockCreateRenderPipelineAsyncCallback {
  public:
    MOCK_METHOD(void,
                Call,
                (WGPUCreatePipelineAsyncStatus status,
                 WGPURenderPipeline pipeline,
                 const char* message,
                 void* userdata));
};

std::unique_ptr<StrictMock<MockCreateRenderPipelineAsyncCallback>>
    mockCreateRenderPipelineAsyncCallback;
void ToMockCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status,
                                             WGPURenderPipeline pipeline,
                                             const char* message,
                                             void* userdata) {
    mockCreateRenderPipelineAsyncCallback->Call(status, pipeline, message, userdata);
}

}  // anonymous namespace

class WireCreatePipelineAsyncTest : public WireTest {
  public:
    void SetUp() override {
        WireTest::SetUp();

        mockCreateComputePipelineAsyncCallback =
            std::make_unique<StrictMock<MockCreateComputePipelineAsyncCallback>>();
        mockCreateRenderPipelineAsyncCallback =
            std::make_unique<StrictMock<MockCreateRenderPipelineAsyncCallback>>();
    }

    void TearDown() override {
        WireTest::TearDown();

        // Delete mock so that expectations are checked
        mockCreateComputePipelineAsyncCallback = nullptr;
        mockCreateRenderPipelineAsyncCallback = nullptr;
    }

    void FlushClient() {
        WireTest::FlushClient();
        Mock::VerifyAndClearExpectations(&mockCreateComputePipelineAsyncCallback);
    }

    void FlushServer() {
        WireTest::FlushServer();
        Mock::VerifyAndClearExpectations(&mockCreateComputePipelineAsyncCallback);
    }
};

// Test when creating a compute pipeline with CreateComputePipelineAsync() successfully.
TEST_F(WireCreatePipelineAsyncTest, CreateComputePipelineAsyncSuccess) {
    WGPUShaderModuleDescriptor csDescriptor{};
    WGPUShaderModule csModule = wgpuDeviceCreateShaderModule(device, &csDescriptor);
    WGPUShaderModule apiCsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiCsModule));

    WGPUComputePipelineDescriptor descriptor{};
    descriptor.compute.module = csModule;
    descriptor.compute.entryPoint = "main";

    wgpuDeviceCreateComputePipelineAsync(device, &descriptor,
                                         ToMockCreateComputePipelineAsyncCallback, this);

    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateComputePipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Success, nullptr, "");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateComputePipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_Success, _, StrEq(""), this))
        .Times(1);

    FlushServer();
}

// Test when creating a compute pipeline with CreateComputePipelineAsync() results in an error.
TEST_F(WireCreatePipelineAsyncTest, CreateComputePipelineAsyncError) {
    WGPUShaderModuleDescriptor csDescriptor{};
    WGPUShaderModule csModule = wgpuDeviceCreateShaderModule(device, &csDescriptor);
    WGPUShaderModule apiCsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiCsModule));

    WGPUComputePipelineDescriptor descriptor{};
    descriptor.compute.module = csModule;
    descriptor.compute.entryPoint = "main";

    wgpuDeviceCreateComputePipelineAsync(device, &descriptor,
                                         ToMockCreateComputePipelineAsyncCallback, this);

    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateComputePipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Error, nullptr, "Some error message");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateComputePipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_Error, _, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test when creating a render pipeline with CreateRenderPipelineAsync() successfully.
TEST_F(WireCreatePipelineAsyncTest, CreateRenderPipelineAsyncSuccess) {
    WGPUShaderModuleDescriptor vertexDescriptor = {};
    WGPUShaderModule vsModule = wgpuDeviceCreateShaderModule(device, &vertexDescriptor);
    WGPUShaderModule apiVsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiVsModule));

    WGPURenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.vertex.module = vsModule;
    pipelineDescriptor.vertex.entryPoint = "main";

    WGPUFragmentState fragment = {};
    fragment.module = vsModule;
    fragment.entryPoint = "main";
    pipelineDescriptor.fragment = &fragment;

    wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDescriptor,
                                        ToMockCreateRenderPipelineAsyncCallback, this);
    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateRenderPipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Success, nullptr, "");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateRenderPipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_Success, _, StrEq(""), this))
        .Times(1);

    FlushServer();
}

// Test when creating a render pipeline with CreateRenderPipelineAsync() results in an error.
TEST_F(WireCreatePipelineAsyncTest, CreateRenderPipelineAsyncError) {
    WGPUShaderModuleDescriptor vertexDescriptor = {};
    WGPUShaderModule vsModule = wgpuDeviceCreateShaderModule(device, &vertexDescriptor);
    WGPUShaderModule apiVsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiVsModule));

    WGPURenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.vertex.module = vsModule;
    pipelineDescriptor.vertex.entryPoint = "main";

    WGPUFragmentState fragment = {};
    fragment.module = vsModule;
    fragment.entryPoint = "main";
    pipelineDescriptor.fragment = &fragment;

    wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDescriptor,
                                        ToMockCreateRenderPipelineAsyncCallback, this);
    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateRenderPipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Error, nullptr, "Some error message");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateRenderPipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_Error, _, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test that registering a callback then wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireCreatePipelineAsyncTest, CreateRenderPipelineAsyncThenDisconnect) {
    WGPUShaderModuleDescriptor vertexDescriptor = {};
    WGPUShaderModule vsModule = wgpuDeviceCreateShaderModule(device, &vertexDescriptor);
    WGPUShaderModule apiVsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiVsModule));

    WGPUFragmentState fragment = {};
    fragment.module = vsModule;
    fragment.entryPoint = "main";

    WGPURenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.vertex.module = vsModule;
    pipelineDescriptor.vertex.entryPoint = "main";
    pipelineDescriptor.fragment = &fragment;

    wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDescriptor,
                                        ToMockCreateRenderPipelineAsyncCallback, this);
    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateRenderPipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Success, nullptr, "");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateRenderPipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_DeviceLost, _, _, this))
        .Times(1);
    GetWireClient()->Disconnect();
}

// Test that registering a callback then wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireCreatePipelineAsyncTest, CreateComputePipelineAsyncThenDisconnect) {
    WGPUShaderModuleDescriptor csDescriptor{};
    WGPUShaderModule csModule = wgpuDeviceCreateShaderModule(device, &csDescriptor);
    WGPUShaderModule apiCsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiCsModule));

    WGPUComputePipelineDescriptor descriptor{};
    descriptor.compute.module = csModule;
    descriptor.compute.entryPoint = "main";

    wgpuDeviceCreateComputePipelineAsync(device, &descriptor,
                                         ToMockCreateComputePipelineAsyncCallback, this);
    EXPECT_CALL(api, OnDeviceCreateComputePipelineAsync(apiDevice, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallDeviceCreateComputePipelineAsyncCallback(
                apiDevice, WGPUCreatePipelineAsyncStatus_Success, nullptr, "");
        }));

    FlushClient();

    EXPECT_CALL(*mockCreateComputePipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_DeviceLost, _, _, this))
        .Times(1);
    GetWireClient()->Disconnect();
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireCreatePipelineAsyncTest, CreateRenderPipelineAsyncAfterDisconnect) {
    WGPUShaderModuleDescriptor vertexDescriptor = {};
    WGPUShaderModule vsModule = wgpuDeviceCreateShaderModule(device, &vertexDescriptor);
    WGPUShaderModule apiVsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiVsModule));

    WGPUFragmentState fragment = {};
    fragment.module = vsModule;
    fragment.entryPoint = "main";

    WGPURenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.vertex.module = vsModule;
    pipelineDescriptor.vertex.entryPoint = "main";
    pipelineDescriptor.fragment = &fragment;

    FlushClient();

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockCreateRenderPipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_DeviceLost, nullptr, _, this))
        .Times(1);
    wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDescriptor,
                                        ToMockCreateRenderPipelineAsyncCallback, this);
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireCreatePipelineAsyncTest, CreateComputePipelineAsyncAfterDisconnect) {
    WGPUShaderModuleDescriptor csDescriptor{};
    WGPUShaderModule csModule = wgpuDeviceCreateShaderModule(device, &csDescriptor);
    WGPUShaderModule apiCsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiCsModule));

    WGPUComputePipelineDescriptor descriptor{};
    descriptor.compute.module = csModule;
    descriptor.compute.entryPoint = "main";

    FlushClient();

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockCreateComputePipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_DeviceLost, nullptr, _, this))
        .Times(1);

    wgpuDeviceCreateComputePipelineAsync(device, &descriptor,
                                         ToMockCreateComputePipelineAsyncCallback, this);
}

TEST_F(WireCreatePipelineAsyncTest, DeviceDeletedBeforeCallback) {
    WGPUShaderModuleDescriptor vertexDescriptor = {};
    WGPUShaderModule module = wgpuDeviceCreateShaderModule(device, &vertexDescriptor);
    WGPUShaderModule apiModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiModule));

    WGPURenderPipelineDescriptor pipelineDescriptor{};
    pipelineDescriptor.vertex.module = module;
    pipelineDescriptor.vertex.entryPoint = "main";

    WGPUFragmentState fragment = {};
    fragment.module = module;
    fragment.entryPoint = "main";
    pipelineDescriptor.fragment = &fragment;

    wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDescriptor,
                                        ToMockCreateRenderPipelineAsyncCallback, this);

    EXPECT_CALL(api, OnDeviceCreateRenderPipelineAsync(apiDevice, _, _, _));
    FlushClient();

    EXPECT_CALL(*mockCreateRenderPipelineAsyncCallback,
                Call(WGPUCreatePipelineAsyncStatus_DeviceDestroyed, nullptr, _, this))
        .Times(1);

    wgpuDeviceRelease(device);

    EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, nullptr, nullptr)).Times(1);
    EXPECT_CALL(api, OnDeviceSetLoggingCallback(apiDevice, nullptr, nullptr)).Times(1);
    EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(apiDevice, nullptr, nullptr)).Times(1);
    EXPECT_CALL(api, DeviceRelease(apiDevice)).Times(1);

    FlushClient();
    DefaultApiDeviceWasReleased();
}

}  // namespace dawn::wire
