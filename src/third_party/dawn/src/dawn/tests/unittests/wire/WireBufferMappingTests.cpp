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

#include <limits>
#include <memory>

#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace {

// Mock class to add expectations on the wire calling callbacks
class MockBufferMapCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUBufferMapAsyncStatus status, void* userdata));
};

std::unique_ptr<StrictMock<MockBufferMapCallback>> mockBufferMapCallback;
void ToMockBufferMapCallback(WGPUBufferMapAsyncStatus status, void* userdata) {
    mockBufferMapCallback->Call(status, userdata);
}

}  // anonymous namespace

class WireBufferMappingTests : public WireTest {
  public:
    WireBufferMappingTests() {}
    ~WireBufferMappingTests() override = default;

    void SetUp() override {
        WireTest::SetUp();

        mockBufferMapCallback = std::make_unique<StrictMock<MockBufferMapCallback>>();
        apiBuffer = api.GetNewBuffer();
    }

    void TearDown() override {
        WireTest::TearDown();

        // Delete mock so that expectations are checked
        mockBufferMapCallback = nullptr;
    }

    void FlushClient() {
        WireTest::FlushClient();
        Mock::VerifyAndClearExpectations(&mockBufferMapCallback);
    }

    void FlushServer() {
        WireTest::FlushServer();
        Mock::VerifyAndClearExpectations(&mockBufferMapCallback);
    }

    void SetupBuffer(WGPUBufferUsageFlags usage) {
        WGPUBufferDescriptor descriptor = {};
        descriptor.size = kBufferSize;
        descriptor.usage = usage;

        buffer = wgpuDeviceCreateBuffer(device, &descriptor);

        EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _))
            .WillOnce(Return(apiBuffer))
            .RetiresOnSaturation();
        FlushClient();
    }

  protected:
    static constexpr uint64_t kBufferSize = sizeof(uint32_t);
    // A successfully created buffer
    WGPUBuffer buffer;
    WGPUBuffer apiBuffer;
};

// Tests specific to mapping for reading
class WireBufferMappingReadTests : public WireBufferMappingTests {
  public:
    WireBufferMappingReadTests() {}
    ~WireBufferMappingReadTests() override = default;

    void SetUp() override {
        WireBufferMappingTests::SetUp();

        SetupBuffer(WGPUBufferUsage_MapRead);
    }
};

// Check mapping for reading a succesfully created buffer
TEST_F(WireBufferMappingReadTests, MappingForReadSuccessBuffer) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServer();

    EXPECT_EQ(bufferContent,
              *static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Check that things work correctly when a validation error happens when mapping the buffer for
// reading
TEST_F(WireBufferMappingReadTests, ErrorWhileMappingForRead) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();

    EXPECT_EQ(nullptr, wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize));
}

// Check that the map read callback is called with UNKNOWN when the buffer is destroyed before
// the request is finished
TEST_F(WireBufferMappingReadTests, DestroyBeforeReadRequestEnd) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    // Return success
    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    // Destroy before the client gets the success, so the callback is called with
    // DestroyedBeforeCallback.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);
    wgpuBufferRelease(buffer);
    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
    FlushServer();
}

// Check the map read callback is called with "UnmappedBeforeCallback" when the map request
// would have worked, but Unmap was called
TEST_F(WireBufferMappingReadTests, UnmapCalledTooEarlyForRead) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    // Oh no! We are calling Unmap too early! However the callback gets fired only after we get
    // an answer from the server.
    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer));

    FlushClient();

    // The callback shouldn't get called with success, even when the request succeeded on the
    // server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_UnmappedBeforeCallback, _))
        .Times(1);

    FlushServer();
}

// Check that even if Unmap() was called early client-side, we correctly surface server-side
// validation errors.
TEST_F(WireBufferMappingReadTests, UnmapCalledTooEarlyForReadButServerSideError) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    // Oh no! We are calling Unmap too early! However the callback gets fired only after we get
    // an answer from the server that the mapAsync call was an error.
    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer));

    FlushClient();

    // The callback should be called with the server-side error and not the
    // UnmappedBeforeCallback.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();
}

// Check the map read callback is called with "DestroyedBeforeCallback" when the map request
// would have worked, but Destroy was called
TEST_F(WireBufferMappingReadTests, DestroyCalledTooEarlyForRead) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    // Oh no! We are calling Unmap too early! However the callback gets fired only after we get
    // an answer from the server.
    wgpuBufferDestroy(buffer);
    EXPECT_CALL(api, BufferDestroy(apiBuffer));

    FlushClient();

    // The callback shouldn't get called with success, even when the request succeeded on the
    // server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);

    FlushServer();
}

// Check that even if Destroy() was called early client-side, we correctly surface server-side
// validation errors.
TEST_F(WireBufferMappingReadTests, DestroyCalledTooEarlyForReadButServerSideError) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    // Oh no! We are calling Destroy too early! However the callback gets fired only after we
    // get an answer from the server that the mapAsync call was an error.
    wgpuBufferDestroy(buffer);
    EXPECT_CALL(api, BufferDestroy(apiBuffer));

    FlushClient();

    // The callback should be called with the server-side error and not the
    // DestroyedBeforCallback..
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();
}

// Check that an error map read while a buffer is already mapped won't changed the result of get
// mapped range
TEST_F(WireBufferMappingReadTests, MappingForReadingErrorWhileAlreadyMappedUnchangeMapData) {
    // Successful map
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServer();

    // Map failure while the buffer is already mapped
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();

    EXPECT_EQ(bufferContent,
              *static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));
}

// Test that the MapReadCallback isn't fired twice when unmap() is called inside the callback
TEST_F(WireBufferMappingReadTests, UnmapInsideMapReadCallback) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&]() { wgpuBufferUnmap(buffer); }));

    FlushServer();

    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test that the MapReadCallback isn't fired twice the buffer external refcount reaches 0 in the
// callback
TEST_F(WireBufferMappingReadTests, DestroyInsideMapReadCallback) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&]() { wgpuBufferRelease(buffer); }));

    FlushServer();

    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
}

// Tests specific to mapping for writing
class WireBufferMappingWriteTests : public WireBufferMappingTests {
  public:
    WireBufferMappingWriteTests() {}
    ~WireBufferMappingWriteTests() override = default;

    void SetUp() override {
        WireBufferMappingTests::SetUp();

        SetupBuffer(WGPUBufferUsage_MapWrite);
    }
};

// Check mapping for writing a succesfully created buffer
TEST_F(WireBufferMappingWriteTests, MappingForWriteSuccessBuffer) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t serverBufferContent = 31337;
    uint32_t updatedContent = 4242;

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&serverBufferContent));

    FlushClient();

    // The map write callback always gets a buffer full of zeroes.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServer();

    uint32_t* lastMapWritePointer =
        static_cast<uint32_t*>(wgpuBufferGetMappedRange(buffer, 0, kBufferSize));
    ASSERT_EQ(0u, *lastMapWritePointer);

    // Write something to the mapped pointer
    *lastMapWritePointer = updatedContent;

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();

    // After the buffer is unmapped, the content of the buffer is updated on the server
    ASSERT_EQ(serverBufferContent, updatedContent);
}

// Check that things work correctly when a validation error happens when mapping the buffer for
// writing
TEST_F(WireBufferMappingWriteTests, ErrorWhileMappingForWrite) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();

    EXPECT_EQ(nullptr, wgpuBufferGetMappedRange(buffer, 0, kBufferSize));
}

// Check that the map write callback is called with "DestroyedBeforeCallback" when the buffer is
// destroyed before the request is finished
TEST_F(WireBufferMappingWriteTests, DestroyBeforeWriteRequestEnd) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    // Return success
    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    // Destroy before the client gets the success, so the callback is called with
    // DestroyedBeforeCallback.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);
    wgpuBufferRelease(buffer);
    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
    FlushServer();
}

// Check the map write callback is called with "UnmappedBeforeCallback" when the map request
// would have worked, but Unmap was called
TEST_F(WireBufferMappingWriteTests, UnmapCalledTooEarlyForWrite) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    // Oh no! We are calling Unmap too early!
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_UnmappedBeforeCallback, _))
        .Times(1);
    wgpuBufferUnmap(buffer);

    // The callback shouldn't get called, even when the request succeeded on the server side
    FlushServer();
}

// Check that an error map write while a buffer is already mapped
TEST_F(WireBufferMappingWriteTests, MappingForWritingErrorWhileAlreadyMapped) {
    // Successful map
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServer();

    // Map failure while the buffer is already mapped
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();

    EXPECT_NE(nullptr,
              static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));
}

// Test that the MapWriteCallback isn't fired twice when unmap() is called inside the callback
TEST_F(WireBufferMappingWriteTests, UnmapInsideMapWriteCallback) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&]() { wgpuBufferUnmap(buffer); }));

    FlushServer();

    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test that the MapWriteCallback isn't fired twice the buffer external refcount reaches 0 in
// the callback
TEST_F(WireBufferMappingWriteTests, DestroyInsideMapWriteCallback) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&]() { wgpuBufferRelease(buffer); }));

    FlushServer();

    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
}

// Test successful buffer creation with mappedAtCreation=true
TEST_F(WireBufferMappingTests, MappedAtCreationSuccess) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test that releasing a buffer mapped at creation does not call Unmap
TEST_F(WireBufferMappingTests, MappedAtCreationReleaseBeforeUnmap) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferRelease(buffer);
    EXPECT_CALL(api, BufferRelease(apiBuffer)).Times(1);

    FlushClient();
}

// Test that it is valid to map a buffer after it is mapped at creation and unmapped
TEST_F(WireBufferMappingTests, MappedAtCreationThenMapSuccess) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.usage = WGPUMapMode_Write;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();

    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&apiBufferData));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServer();
}

// Test that it is invalid to map a buffer after mappedAtCreation but before Unmap
TEST_F(WireBufferMappingTests, MappedAtCreationThenMapFailure) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&]() { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Error); }));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Error, _)).Times(1);

    FlushServer();

    EXPECT_NE(nullptr,
              static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Check that trying to create a buffer of size MAX_SIZE_T is an error handling in the client
// and never gets to the server-side.
TEST_F(WireBufferMappingTests, MaxSizeMappableBufferOOMDirectly) {
    size_t kOOMSize = std::numeric_limits<size_t>::max();
    WGPUBuffer apiBuffer = api.GetNewBuffer();

    // Check for CreateBufferMapped.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_CopySrc;
        descriptor.size = kOOMSize;
        descriptor.mappedAtCreation = true;

        wgpuDeviceCreateBuffer(device, &descriptor);
        EXPECT_CALL(api, DeviceInjectError(apiDevice, WGPUErrorType_OutOfMemory, _));
        EXPECT_CALL(api, DeviceCreateErrorBuffer(apiDevice)).WillOnce(Return(apiBuffer));
        FlushClient();
    }

    // Check for MapRead usage.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_MapRead;
        descriptor.size = kOOMSize;

        wgpuDeviceCreateBuffer(device, &descriptor);
        EXPECT_CALL(api, DeviceInjectError(apiDevice, WGPUErrorType_OutOfMemory, _));
        EXPECT_CALL(api, DeviceCreateErrorBuffer(apiDevice)).WillOnce(Return(apiBuffer));
        FlushClient();
    }

    // Check for MapWrite usage.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_MapWrite;
        descriptor.size = kOOMSize;

        wgpuDeviceCreateBuffer(device, &descriptor);
        EXPECT_CALL(api, DeviceInjectError(apiDevice, WGPUErrorType_OutOfMemory, _));
        EXPECT_CALL(api, DeviceCreateErrorBuffer(apiDevice)).WillOnce(Return(apiBuffer));
        FlushClient();
    }
}

// Test that registering a callback then wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireBufferMappingTests, MapThenDisconnect) {
    SetupBuffer(WGPUMapMode_Write);
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, this);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize)).Times(1);

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, this)).Times(1);
    GetWireClient()->Disconnect();
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireBufferMappingTests, MapAfterDisconnect) {
    SetupBuffer(WGPUMapMode_Read);

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, this)).Times(1);
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, this);
}

// Hack to pass in test context into user callback
struct TestData {
    WireBufferMappingTests* pTest;
    WGPUBuffer* pTestBuffer;
    size_t numRequests;
};

static void ToMockBufferMapCallbackWithNewRequests(WGPUBufferMapAsyncStatus status,
                                                   void* userdata) {
    TestData* testData = reinterpret_cast<TestData*>(userdata);
    // Mimic the user callback is sending new requests
    ASSERT_NE(testData, nullptr);
    ASSERT_NE(testData->pTest, nullptr);
    ASSERT_NE(testData->pTestBuffer, nullptr);

    mockBufferMapCallback->Call(status, testData->pTest);

    // Send the requests a number of times
    for (size_t i = 0; i < testData->numRequests; i++) {
        wgpuBufferMapAsync(*(testData->pTestBuffer), WGPUMapMode_Write, 0, sizeof(uint32_t),
                           ToMockBufferMapCallback, testData->pTest);
    }
}

// Test that requests inside user callbacks before disconnect are called
TEST_F(WireBufferMappingTests, MapInsideCallbackBeforeDisconnect) {
    SetupBuffer(WGPUMapMode_Write);
    TestData testData = {this, &buffer, 10};
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize,
                       ToMockBufferMapCallbackWithNewRequests, &testData);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize)).Times(1);

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, this))
        .Times(1 + testData.numRequests);
    GetWireClient()->Disconnect();
}

// Test that requests inside user callbacks before object destruction are called
TEST_F(WireBufferMappingWriteTests, MapInsideCallbackBeforeDestruction) {
    TestData testData = {this, &buffer, 10};
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize,
                       ToMockBufferMapCallbackWithNewRequests, &testData);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
        }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize)).Times(1);

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback,
                Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, this))
        .Times(1 + testData.numRequests);
    wgpuBufferRelease(buffer);
}

}  // namespace dawn::wire
