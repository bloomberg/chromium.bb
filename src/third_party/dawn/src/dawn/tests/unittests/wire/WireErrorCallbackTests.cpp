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

#include <memory>

#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::StrictMock;

namespace {

// Mock classes to add expectations on the wire calling callbacks
class MockDeviceErrorCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUErrorType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceErrorCallback>> mockDeviceErrorCallback;
void ToMockDeviceErrorCallback(WGPUErrorType type, const char* message, void* userdata) {
    mockDeviceErrorCallback->Call(type, message, userdata);
}

class MockDevicePopErrorScopeCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUErrorType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDevicePopErrorScopeCallback>> mockDevicePopErrorScopeCallback;
void ToMockDevicePopErrorScopeCallback(WGPUErrorType type, const char* message, void* userdata) {
    mockDevicePopErrorScopeCallback->Call(type, message, userdata);
}

class MockDeviceLoggingCallback {
  public:
    MOCK_METHOD(void, Call, (WGPULoggingType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceLoggingCallback>> mockDeviceLoggingCallback;
void ToMockDeviceLoggingCallback(WGPULoggingType type, const char* message, void* userdata) {
    mockDeviceLoggingCallback->Call(type, message, userdata);
}

class MockDeviceLostCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUDeviceLostReason reason, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceLostCallback>> mockDeviceLostCallback;
void ToMockDeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void* userdata) {
    mockDeviceLostCallback->Call(reason, message, userdata);
}

}  // anonymous namespace

class WireErrorCallbackTests : public WireTest {
  public:
    WireErrorCallbackTests() {}
    ~WireErrorCallbackTests() override = default;

    void SetUp() override {
        WireTest::SetUp();

        mockDeviceErrorCallback = std::make_unique<StrictMock<MockDeviceErrorCallback>>();
        mockDeviceLoggingCallback = std::make_unique<StrictMock<MockDeviceLoggingCallback>>();
        mockDevicePopErrorScopeCallback =
            std::make_unique<StrictMock<MockDevicePopErrorScopeCallback>>();
        mockDeviceLostCallback = std::make_unique<StrictMock<MockDeviceLostCallback>>();
    }

    void TearDown() override {
        WireTest::TearDown();

        mockDeviceErrorCallback = nullptr;
        mockDeviceLoggingCallback = nullptr;
        mockDevicePopErrorScopeCallback = nullptr;
        mockDeviceLostCallback = nullptr;
    }

    void FlushServer() {
        WireTest::FlushServer();

        Mock::VerifyAndClearExpectations(&mockDeviceErrorCallback);
        Mock::VerifyAndClearExpectations(&mockDevicePopErrorScopeCallback);
    }
};

// Test the return wire for device error callbacks
TEST_F(WireErrorCallbackTests, DeviceErrorCallback) {
    wgpuDeviceSetUncapturedErrorCallback(device, ToMockDeviceErrorCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetUncapturedErrorCallbackCallback(apiDevice, WGPUErrorType_Validation,
                                                     "Some error message");

    EXPECT_CALL(*mockDeviceErrorCallback,
                Call(WGPUErrorType_Validation, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for device user warning callbacks
TEST_F(WireErrorCallbackTests, DeviceLoggingCallback) {
    wgpuDeviceSetLoggingCallback(device, ToMockDeviceLoggingCallback, this);

    // Setting the injected warning callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetLoggingCallbackCallback(apiDevice, WGPULoggingType_Info, "Some message");

    EXPECT_CALL(*mockDeviceLoggingCallback, Call(WGPULoggingType_Info, StrEq("Some message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for error scopes.
TEST_F(WireErrorCallbackTests, PushPopErrorScopeCallback) {
    EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(1);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    FlushClient();

    WGPUErrorCallback callback;
    void* userdata;
    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _))
        .WillOnce(DoAll(SaveArg<1>(&callback), SaveArg<2>(&userdata), Return(true)));
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
    FlushClient();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_Validation, StrEq("Some error message"), this))
        .Times(1);
    callback(WGPUErrorType_Validation, "Some error message", userdata);
    FlushServer();
}

// Test the return wire for error scopes when callbacks return in a various orders.
TEST_F(WireErrorCallbackTests, PopErrorScopeCallbackOrdering) {
    // Two error scopes are popped, and the first one returns first.
    {
        EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(2);
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        FlushClient();

        WGPUErrorCallback callback1;
        WGPUErrorCallback callback2;
        void* userdata1;
        void* userdata2;
        EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _))
            .WillOnce(DoAll(SaveArg<1>(&callback1), SaveArg<2>(&userdata1), Return(true)))
            .WillOnce(DoAll(SaveArg<1>(&callback2), SaveArg<2>(&userdata2), Return(true)));
        wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
        wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this + 1);
        FlushClient();

        EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                    Call(WGPUErrorType_Validation, StrEq("First error message"), this))
            .Times(1);
        callback1(WGPUErrorType_Validation, "First error message", userdata1);
        FlushServer();

        EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                    Call(WGPUErrorType_Validation, StrEq("Second error message"), this + 1))
            .Times(1);
        callback2(WGPUErrorType_Validation, "Second error message", userdata2);
        FlushServer();
    }

    // Two error scopes are popped, and the second one returns first.
    {
        EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(2);
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        FlushClient();

        WGPUErrorCallback callback1;
        WGPUErrorCallback callback2;
        void* userdata1;
        void* userdata2;
        EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _))
            .WillOnce(DoAll(SaveArg<1>(&callback1), SaveArg<2>(&userdata1), Return(true)))
            .WillOnce(DoAll(SaveArg<1>(&callback2), SaveArg<2>(&userdata2), Return(true)));
        wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
        wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this + 1);
        FlushClient();

        EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                    Call(WGPUErrorType_Validation, StrEq("Second error message"), this + 1))
            .Times(1);
        callback2(WGPUErrorType_Validation, "Second error message", userdata2);
        FlushServer();

        EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                    Call(WGPUErrorType_Validation, StrEq("First error message"), this))
            .Times(1);
        callback1(WGPUErrorType_Validation, "First error message", userdata1);
        FlushServer();
    }
}

// Test the return wire for error scopes in flight when the device is destroyed.
TEST_F(WireErrorCallbackTests, PopErrorScopeDeviceInFlightDestroy) {
    EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(1);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    FlushClient();

    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _)).WillOnce(Return(true));
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
    FlushClient();

    // Incomplete callback called in Device destructor. This is resolved after the end of this
    // test.
    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_Unknown, ValidStringMessage(), this))
        .Times(1);
}

// Test that registering a callback then wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireErrorCallbackTests, PopErrorScopeThenDisconnect) {
    EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(1);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);

    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _)).WillOnce(Return(true));
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
    FlushClient();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_DeviceLost, ValidStringMessage(), this))
        .Times(1);
    GetWireClient()->Disconnect();
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireErrorCallbackTests, PopErrorScopeAfterDisconnect) {
    EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(1);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    FlushClient();

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_DeviceLost, ValidStringMessage(), this))
        .Times(1);
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
}

// Empty stack (We are emulating the errors that would be callback-ed from native).
TEST_F(WireErrorCallbackTests, PopErrorScopeEmptyStack) {
    WGPUErrorCallback callback;
    void* userdata;
    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _))
        .WillOnce(DoAll(SaveArg<1>(&callback), SaveArg<2>(&userdata), Return(true)));
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
    FlushClient();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_Validation, StrEq("No error scopes to pop"), this))
        .Times(1);
    callback(WGPUErrorType_Validation, "No error scopes to pop", userdata);
    FlushServer();
}

// Test the return wire for device lost callback
TEST_F(WireErrorCallbackTests, DeviceLostCallback) {
    wgpuDeviceSetDeviceLostCallback(device, ToMockDeviceLostCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetDeviceLostCallbackCallback(apiDevice, WGPUDeviceLostReason_Undefined,
                                                "Some error message");

    EXPECT_CALL(*mockDeviceLostCallback,
                Call(WGPUDeviceLostReason_Undefined, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

}  // namespace dawn::wire
