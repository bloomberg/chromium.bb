// Copyright 2019 The Dawn Authors
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

#include "dawn/tests/unittests/wire/WireTest.h"

#include "dawn/dawn_proc.h"
#include "dawn/utils/TerribleCommandBuffer.h"
#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireServer.h"

using testing::_;
using testing::AnyNumber;
using testing::Exactly;
using testing::Mock;
using testing::Return;

WireTest::WireTest() {}

WireTest::~WireTest() {}

dawn::wire::client::MemoryTransferService* WireTest::GetClientMemoryTransferService() {
    return nullptr;
}

dawn::wire::server::MemoryTransferService* WireTest::GetServerMemoryTransferService() {
    return nullptr;
}

void WireTest::SetUp() {
    DawnProcTable mockProcs;
    api.GetProcTable(&mockProcs);
    WGPUDevice mockDevice = api.GetNewDevice();

    // This SetCallback call cannot be ignored because it is done as soon as we start the server
    EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(_, _, _)).Times(Exactly(1));
    EXPECT_CALL(api, OnDeviceSetLoggingCallback(_, _, _)).Times(Exactly(1));
    EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(_, _, _)).Times(Exactly(1));
    SetupIgnoredCallExpectations();

    mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();
    mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>(mWireServer.get());

    dawn::wire::WireServerDescriptor serverDesc = {};
    serverDesc.procs = &mockProcs;
    serverDesc.serializer = mS2cBuf.get();
    serverDesc.memoryTransferService = GetServerMemoryTransferService();

    mWireServer.reset(new dawn::wire::WireServer(serverDesc));
    mC2sBuf->SetHandler(mWireServer.get());

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = mC2sBuf.get();
    clientDesc.memoryTransferService = GetClientMemoryTransferService();

    mWireClient.reset(new dawn::wire::WireClient(clientDesc));
    mS2cBuf->SetHandler(mWireClient.get());

    dawnProcSetProcs(&dawn::wire::client::GetProcs());

    auto deviceReservation = mWireClient->ReserveDevice();
    EXPECT_CALL(api, DeviceReference(mockDevice));
    mWireServer->InjectDevice(mockDevice, deviceReservation.id, deviceReservation.generation);

    device = deviceReservation.device;
    apiDevice = mockDevice;

    // The GetQueue is done on WireClient startup so we expect it now.
    queue = wgpuDeviceGetQueue(device);
    apiQueue = api.GetNewQueue();
    EXPECT_CALL(api, DeviceGetQueue(apiDevice)).WillOnce(Return(apiQueue));
    FlushClient();
}

void WireTest::TearDown() {
    dawnProcSetProcs(nullptr);

    // Derived classes should call the base TearDown() first. The client must
    // be reset before any mocks are deleted.
    // Incomplete client callbacks will be called on deletion, so the mocks
    // cannot be null.
    api.IgnoreAllReleaseCalls();
    mWireClient = nullptr;

    if (mWireServer && apiDevice) {
        // These are called on server destruction to clear the callbacks. They must not be
        // called after the server is destroyed.
        EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, nullptr, nullptr))
            .Times(Exactly(1));
        EXPECT_CALL(api, OnDeviceSetLoggingCallback(apiDevice, nullptr, nullptr)).Times(Exactly(1));
        EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(apiDevice, nullptr, nullptr))
            .Times(Exactly(1));
    }
    mWireServer = nullptr;
}

// This should be called if |apiDevice| is no longer exists on the wire.
// This signals that expectations in |TearDowb| shouldn't be added.
void WireTest::DefaultApiDeviceWasReleased() {
    apiDevice = nullptr;
}

void WireTest::FlushClient(bool success) {
    ASSERT_EQ(mC2sBuf->Flush(), success);

    Mock::VerifyAndClearExpectations(&api);
    SetupIgnoredCallExpectations();
}

void WireTest::FlushServer(bool success) {
    ASSERT_EQ(mS2cBuf->Flush(), success);
}

dawn::wire::WireServer* WireTest::GetWireServer() {
    return mWireServer.get();
}

dawn::wire::WireClient* WireTest::GetWireClient() {
    return mWireClient.get();
}

void WireTest::DeleteServer() {
    EXPECT_CALL(api, QueueRelease(apiQueue)).Times(1);
    EXPECT_CALL(api, DeviceRelease(apiDevice)).Times(1);

    if (mWireServer) {
        // These are called on server destruction to clear the callbacks. They must not be
        // called after the server is destroyed.
        EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, nullptr, nullptr))
            .Times(Exactly(1));
        EXPECT_CALL(api, OnDeviceSetLoggingCallback(apiDevice, nullptr, nullptr)).Times(Exactly(1));
        EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(apiDevice, nullptr, nullptr))
            .Times(Exactly(1));
    }
    mWireServer = nullptr;
}

void WireTest::DeleteClient() {
    mWireClient = nullptr;
}

void WireTest::SetupIgnoredCallExpectations() {
    EXPECT_CALL(api, DeviceTick(_)).Times(AnyNumber());
}
