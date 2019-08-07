// Copyright 2017 The Dawn Authors
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

#include "tests/DawnTest.h"

#include <cstring>

class BufferMapReadTests : public DawnTest {
    protected:
      static void MapReadCallback(DawnBufferMapAsyncStatus status,
                                  const void* data,
                                  uint64_t,
                                  DawnCallbackUserdata userdata) {
          ASSERT_EQ(DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, status);
          ASSERT_NE(nullptr, data);

          auto test = reinterpret_cast<BufferMapReadTests*>(static_cast<uintptr_t>(userdata));
          test->mappedData = data;
      }

      const void* MapReadAsyncAndWait(const dawn::Buffer& buffer) {
          buffer.MapReadAsync(MapReadCallback, static_cast<dawn::CallbackUserdata>(
                                                   reinterpret_cast<uintptr_t>(this)));

          while (mappedData == nullptr) {
              WaitABit();
          }

          return mappedData;
      }

    private:
        const void* mappedData = nullptr;
};

// Test that the simplest map read works.
TEST_P(BufferMapReadTests, SmallReadAtZero) {
    dawn::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t myData = 0x01020304;
    buffer.SetSubData(0, sizeof(myData), reinterpret_cast<uint8_t*>(&myData));

    const void* mappedData = MapReadAsyncAndWait(buffer);
    ASSERT_EQ(myData, *reinterpret_cast<const uint32_t*>(mappedData));

    buffer.Unmap();
}

// Test mapping a large buffer.
TEST_P(BufferMapReadTests, LargeRead) {
    constexpr uint32_t kDataSize = 1000 * 1000;
    std::vector<uint32_t> myData;
    for (uint32_t i = 0; i < kDataSize; ++i) {
        myData.push_back(i);
    }

    dawn::BufferDescriptor descriptor;
    descriptor.size = static_cast<uint32_t>(kDataSize * sizeof(uint32_t));
    descriptor.usage = dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    buffer.SetSubData(0, kDataSize * sizeof(uint32_t), reinterpret_cast<uint8_t*>(myData.data()));

    const void* mappedData = MapReadAsyncAndWait(buffer);
    ASSERT_EQ(0, memcmp(mappedData, myData.data(), kDataSize * sizeof(uint32_t)));

    buffer.Unmap();
}

DAWN_INSTANTIATE_TEST(BufferMapReadTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);

class BufferMapWriteTests : public DawnTest {
    protected:
      static void MapWriteCallback(DawnBufferMapAsyncStatus status,
                                   void* data,
                                   uint64_t,
                                   DawnCallbackUserdata userdata) {
          ASSERT_EQ(DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, status);
          ASSERT_NE(nullptr, data);

          auto test = reinterpret_cast<BufferMapWriteTests*>(static_cast<uintptr_t>(userdata));
          test->mappedData = data;
      }

      void* MapWriteAsyncAndWait(const dawn::Buffer& buffer) {
          buffer.MapWriteAsync(MapWriteCallback, static_cast<dawn::CallbackUserdata>(
                                                     reinterpret_cast<uintptr_t>(this)));

          while (mappedData == nullptr) {
              WaitABit();
          }

          return mappedData;
      }

    private:
        void* mappedData = nullptr;
};

// Test that the simplest map write works.
TEST_P(BufferMapWriteTests, SmallWriteAtZero) {
    dawn::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t myData = 2934875;
    void* mappedData = MapWriteAsyncAndWait(buffer);
    memcpy(mappedData, &myData, sizeof(myData));
    buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(myData, buffer, 0);
}

// Test mapping a large buffer.
TEST_P(BufferMapWriteTests, LargeWrite) {
    constexpr uint32_t kDataSize = 1000 * 1000;
    std::vector<uint32_t> myData;
    for (uint32_t i = 0; i < kDataSize; ++i) {
        myData.push_back(i);
    }

    dawn::BufferDescriptor descriptor;
    descriptor.size = static_cast<uint32_t>(kDataSize * sizeof(uint32_t));
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    void* mappedData = MapWriteAsyncAndWait(buffer);
    memcpy(mappedData, myData.data(), kDataSize * sizeof(uint32_t));
    buffer.Unmap();

    EXPECT_BUFFER_U32_RANGE_EQ(myData.data(), buffer, 0, kDataSize);
}

DAWN_INSTANTIATE_TEST(BufferMapWriteTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);

class BufferSetSubDataTests : public DawnTest {
};

// Test the simplest set sub data: setting one u32 at offset 0.
TEST_P(BufferSetSubDataTests, SmallDataAtZero) {
    dawn::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t value = 0x01020304;
    buffer.SetSubData(0, sizeof(value), reinterpret_cast<uint8_t*>(&value));

    EXPECT_BUFFER_U32_EQ(value, buffer, 0);
}

// Test that SetSubData offset works.
TEST_P(BufferSetSubDataTests, SmallDataAtOffset) {
    dawn::BufferDescriptor descriptor;
    descriptor.size = 4000;
    descriptor.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    constexpr uint64_t kOffset = 2000;
    uint32_t value = 0x01020304;
    buffer.SetSubData(kOffset, sizeof(value), reinterpret_cast<uint8_t*>(&value));

    EXPECT_BUFFER_U32_EQ(value, buffer, kOffset);
}

// Stress test for many calls to SetSubData
TEST_P(BufferSetSubDataTests, ManySetSubData) {
    // Test failing on Mac Metal Intel, maybe because Metal runs out of space to encode commands.
    // See https://bugs.chromium.org/p/dawn/issues/detail?id=108
    DAWN_SKIP_TEST_IF(IsMacOS() && IsMetal() && IsIntel());

    // Note: Increasing the size of the buffer will likely cause timeout issues.
    // In D3D12, timeout detection occurs when the GPU scheduler tries but cannot preempt the task
    // executing these commands in-flight. If this takes longer than ~2s, a device reset occurs and
    // fails the test. Since GPUs may or may not complete by then, this test must be disabled OR
    // modified to be well-below the timeout limit.
    constexpr uint64_t kSize = 4000 * 1000;
    constexpr uint32_t kElements = 500 * 500;
    dawn::BufferDescriptor descriptor;
    descriptor.size = kSize;
    descriptor.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    std::vector<uint32_t> expectedData;
    for (uint32_t i = 0; i < kElements; ++i) {
        buffer.SetSubData(i * sizeof(uint32_t), sizeof(i), reinterpret_cast<uint8_t*>(&i));
        expectedData.push_back(i);
    }

    EXPECT_BUFFER_U32_RANGE_EQ(expectedData.data(), buffer, 0, kElements);
}

// Test using SetSubData for lots of data
TEST_P(BufferSetSubDataTests, LargeSetSubData) {
    constexpr uint64_t kSize = 4000 * 1000;
    constexpr uint32_t kElements = 1000 * 1000;
    dawn::BufferDescriptor descriptor;
    descriptor.size = kSize;
    descriptor.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    std::vector<uint32_t> expectedData;
    for (uint32_t i = 0; i < kElements; ++i) {
        expectedData.push_back(i);
    }

    buffer.SetSubData(0, kElements * sizeof(uint32_t), reinterpret_cast<uint8_t*>(expectedData.data()));

    EXPECT_BUFFER_U32_RANGE_EQ(expectedData.data(), buffer, 0, kElements);
}

DAWN_INSTANTIATE_TEST(BufferSetSubDataTests,
                     D3D12Backend,
                     MetalBackend,
                     OpenGLBackend,
                     VulkanBackend);

class CreateBufferMappedTests : public DawnTest {};

// Test that the simplest CreateBufferMapped works.
TEST_P(CreateBufferMappedTests, SmallSyncWrite) {
    dawn::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;

    uint32_t myData = 230502;
    dawn::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);
    ASSERT_EQ(result.dataLength, descriptor.size);
    memcpy(result.data, &myData, sizeof(myData));
    result.buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(myData, result.buffer, 0);
}

// Test CreateBufferMapped for a large buffer
TEST_P(CreateBufferMappedTests, LargeSyncWrite) {
    constexpr uint64_t kDataSize = 1000 * 1000;
    std::vector<uint32_t> myData;
    for (uint32_t i = 0; i < kDataSize; ++i) {
        myData.push_back(i);
    }

    dawn::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = static_cast<uint64_t>(kDataSize * sizeof(uint32_t));
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;

    dawn::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);
    ASSERT_EQ(result.dataLength, descriptor.size);
    memcpy(result.data, myData.data(), kDataSize * sizeof(uint32_t));
    result.buffer.Unmap();

    EXPECT_BUFFER_U32_RANGE_EQ(myData.data(), result.buffer, 0, kDataSize);
}

// Test that CreateBufferMapped returns zero-initialized data
// TODO(enga): This should use the testing toggle to initialize resources to 1.
TEST_P(CreateBufferMappedTests, ZeroInitialized) {
    dawn::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;

    dawn::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);
    ASSERT_EQ(result.dataLength, descriptor.size);
    ASSERT_EQ(*result.data, 0);
    result.buffer.Unmap();
}

// Test that mapping a buffer is valid after CreateBufferMapped and Unmap
TEST_P(CreateBufferMappedTests, CreateThenMapSuccess) {
    dawn::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;

    static uint32_t myData = 230502;
    static uint32_t myData2 = 1337;
    dawn::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);
    ASSERT_EQ(result.dataLength, descriptor.size);
    memcpy(result.data, &myData, sizeof(myData));
    result.buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(myData, result.buffer, 0);

    bool done = false;
    result.buffer.MapWriteAsync(
        [](DawnBufferMapAsyncStatus status, void* data, uint64_t, DawnCallbackUserdata userdata) {
            ASSERT_EQ(DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, status);
            ASSERT_NE(nullptr, data);

            *reinterpret_cast<uint32_t*>(data) = myData2;
            *reinterpret_cast<bool*>(static_cast<uintptr_t>(userdata)) = true;
        },
        static_cast<DawnCallbackUserdata>(reinterpret_cast<uintptr_t>(&done)));

    while (!done) {
        WaitABit();
    }

    result.buffer.Unmap();
    EXPECT_BUFFER_U32_EQ(myData2, result.buffer, 0);
}

// Test that is is invalid to map a buffer twice when using CreateBufferMapped
TEST_P(CreateBufferMappedTests, CreateThenMapBeforeUnmapFailure) {
    dawn::BufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;

    uint32_t myData = 230502;
    dawn::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);
    ASSERT_EQ(result.dataLength, descriptor.size);
    memcpy(result.data, &myData, sizeof(myData));

    ASSERT_DEVICE_ERROR([&]() {
        bool done = false;
        result.buffer.MapWriteAsync(
            [](DawnBufferMapAsyncStatus status, void* data, uint64_t,
               DawnCallbackUserdata userdata) {
                ASSERT_EQ(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, status);
                ASSERT_EQ(nullptr, data);

                *reinterpret_cast<bool*>(static_cast<uintptr_t>(userdata)) = true;
            },
            static_cast<DawnCallbackUserdata>(reinterpret_cast<uintptr_t>(&done)));

        while (!done) {
            WaitABit();
        }
    }());

    // CreateBufferMapped is unaffected by the MapWrite error.
    result.buffer.Unmap();
    EXPECT_BUFFER_U32_EQ(myData, result.buffer, 0);
}

DAWN_INSTANTIATE_TEST(CreateBufferMappedTests,
                      D3D12Backend,
                      MetalBackend,
                      OpenGLBackend,
                      VulkanBackend);
