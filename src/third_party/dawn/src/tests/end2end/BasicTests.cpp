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

#include "utils/DawnHelpers.h"

class BasicTests : public DawnTest {
};

// Test Buffer::SetSubData changes the content of the buffer, but really this is the most
// basic test possible, and tests the test harness
TEST_P(BasicTests, BufferSetSubData) {
    dawn::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst;
    dawn::Buffer buffer = device.CreateBuffer(&descriptor);

    uint8_t value = 187;
    buffer.SetSubData(0, sizeof(value), &value);

    EXPECT_BUFFER_U8_EQ(value, buffer, 0);
}

DAWN_INSTANTIATE_TEST(BasicTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend)
