// Copyright 2022 The Dawn Authors
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

#include <utility>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

class DeviceLifetimeTests : public DawnTest {};

// Test that the device can be dropped before its queue.
TEST_P(DeviceLifetimeTests, DroppedBeforeQueue) {
    wgpu::Queue queue = device.GetQueue();

    device = nullptr;
}

// Test that the device can be dropped while an onSubmittedWorkDone callback is in flight.
TEST_P(DeviceLifetimeTests, DroppedWhileQueueOnSubmittedWorkDone) {
    // Submit some work.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(nullptr);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Ask for an onSubmittedWorkDone callback and drop the device.
    queue.OnSubmittedWorkDone(
        0,
        [](WGPUQueueWorkDoneStatus status, void*) {
            EXPECT_EQ(status, WGPUQueueWorkDoneStatus_Success);
        },
        nullptr);

    device = nullptr;
}

// Test that the device can be dropped inside an onSubmittedWorkDone callback.
TEST_P(DeviceLifetimeTests, DroppedInsideQueueOnSubmittedWorkDone) {
    // Submit some work.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(nullptr);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    struct Userdata {
        wgpu::Device device;
        bool done;
    };
    // Ask for an onSubmittedWorkDone callback and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), false};
    queue.OnSubmittedWorkDone(
        0,
        [](WGPUQueueWorkDoneStatus status, void* userdata) {
            EXPECT_EQ(status, WGPUQueueWorkDoneStatus_Success);
            static_cast<Userdata*>(userdata)->device = nullptr;
            static_cast<Userdata*>(userdata)->done = true;
        },
        &data);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }
}

// Test that the device can be dropped while a popErrorScope callback is in flight.
TEST_P(DeviceLifetimeTests, DroppedWhilePopErrorScope) {
    device.PushErrorScope(wgpu::ErrorFilter::Validation);
    bool wire = UsesWire();
    device.PopErrorScope(
        [](WGPUErrorType type, const char*, void* userdata) {
            const bool wire = *static_cast<bool*>(userdata);
            // On the wire, all callbacks get rejected immediately with once the device is deleted.
            // In native, popErrorScope is called synchronously.
            // TODO(crbug.com/dawn/1122): These callbacks should be made consistent.
            EXPECT_EQ(type, wire ? WGPUErrorType_Unknown : WGPUErrorType_NoError);
        },
        &wire);
    device = nullptr;
}

// Test that the device can be dropped inside an onSubmittedWorkDone callback.
TEST_P(DeviceLifetimeTests, DroppedInsidePopErrorScope) {
    struct Userdata {
        wgpu::Device device;
        bool done;
    };
    device.PushErrorScope(wgpu::ErrorFilter::Validation);

    // Ask for a popErrorScope callback and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), false};
    data.device.PopErrorScope(
        [](WGPUErrorType type, const char*, void* userdata) {
            EXPECT_EQ(type, WGPUErrorType_NoError);
            static_cast<Userdata*>(userdata)->device = nullptr;
            static_cast<Userdata*>(userdata)->done = true;
        },
        &data);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }
}

// Test that the device can be dropped before a buffer created from it.
TEST_P(DeviceLifetimeTests, DroppedBeforeBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    device = nullptr;
}

// Test that the device can be dropped while a buffer created from it is being mapped.
TEST_P(DeviceLifetimeTests, DroppedWhileMappingBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    buffer.MapAsync(
        wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
        [](WGPUBufferMapAsyncStatus status, void*) {
            EXPECT_EQ(status, WGPUBufferMapAsyncStatus_DestroyedBeforeCallback);
        },
        nullptr);

    device = nullptr;
}

// Test that the device can be dropped before a mapped buffer created from it.
TEST_P(DeviceLifetimeTests, DroppedBeforeMappedBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    bool done = false;
    buffer.MapAsync(
        wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(status, WGPUBufferMapAsyncStatus_Success);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }

    device = nullptr;
}

// Test that the device can be dropped before a mapped at creation buffer created from it.
TEST_P(DeviceLifetimeTests, DroppedBeforeMappedAtCreationBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    desc.mappedAtCreation = true;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    device = nullptr;
}

// Test that the device can be dropped before a buffer created from it, then mapping the buffer
// fails.
TEST_P(DeviceLifetimeTests, DroppedThenMapBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    device = nullptr;

    bool done = false;
    buffer.MapAsync(
        wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(status, WGPUBufferMapAsyncStatus_DeviceLost);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }
}

// Test that the device can be dropped inside a buffer map callback.
TEST_P(DeviceLifetimeTests, DroppedInsideBufferMapCallback) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    struct Userdata {
        wgpu::Device device;
        wgpu::Buffer buffer;
        bool wire;
        bool done;
    };

    // Ask for a mapAsync callback and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), buffer, UsesWire(), false};
    buffer.MapAsync(
        wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(status, WGPUBufferMapAsyncStatus_Success);
            auto* data = static_cast<Userdata*>(userdata);
            data->device = nullptr;
            data->done = true;

            // Mapped data should be null since the buffer is implicitly destroyed.
            // TODO(crbug.com/dawn/1424): On the wire client, we don't track device child objects so
            // the mapped data is still available when the device is destroyed.
            if (!data->wire) {
                EXPECT_EQ(data->buffer.GetConstMappedRange(), nullptr);
            }
        },
        &data);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }

    // Mapped data should be null since the buffer is implicitly destroyed.
    // TODO(crbug.com/dawn/1424): On the wire client, we don't track device child objects so the
    // mapped data is still available when the device is destroyed.
    if (!UsesWire()) {
        EXPECT_EQ(buffer.GetConstMappedRange(), nullptr);
    }
}

// Test that the device can be dropped while a write buffer operation is enqueued.
TEST_P(DeviceLifetimeTests, DroppedWhileWriteBuffer) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    uint32_t value = 7;
    queue.WriteBuffer(buffer, 0, &value, sizeof(value));
    device = nullptr;
}

// Test that the device can be dropped while a write buffer operation is enqueued and then
// a queue submit occurs. This is slightly different from the former test since it ensures
// that pending work is flushed.
TEST_P(DeviceLifetimeTests, DroppedWhileWriteBufferAndSubmit) {
    wgpu::BufferDescriptor desc = {};
    desc.size = 4;
    desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&desc);

    uint32_t value = 7;
    queue.WriteBuffer(buffer, 0, &value, sizeof(value));
    queue.Submit(0, nullptr);
    device = nullptr;
}

// Test that the device can be dropped while createPipelineAsync is in flight
TEST_P(DeviceLifetimeTests, DroppedWhileCreatePipelineAsync) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char* message,
           void* userdata) {
            wgpu::ComputePipeline::Acquire(cPipeline);
            EXPECT_EQ(status, WGPUCreatePipelineAsyncStatus_DeviceDestroyed);
        },
        nullptr);

    device = nullptr;
}

// Test that the device can be dropped inside a createPipelineAsync callback
TEST_P(DeviceLifetimeTests, DroppedInsideCreatePipelineAsync) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    struct Userdata {
        wgpu::Device device;
        bool done;
    };
    // Call CreateComputePipelineAsync and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), false};
    data.device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char* message,
           void* userdata) {
            wgpu::ComputePipeline::Acquire(cPipeline);
            EXPECT_EQ(status, WGPUCreatePipelineAsyncStatus_Success);

            static_cast<Userdata*>(userdata)->device = nullptr;
            static_cast<Userdata*>(userdata)->done = true;
        },
        &data);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }
}

// Test that the device can be dropped while createPipelineAsync which will hit the frontend cache
// is in flight
TEST_P(DeviceLifetimeTests, DroppedWhileCreatePipelineAsyncAlreadyCached) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    // Create a pipeline ahead of time so it's in the cache.
    wgpu::ComputePipeline p = device.CreateComputePipeline(&desc);

    bool wire = UsesWire();
    device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char*,
           void* userdata) {
            const bool wire = *static_cast<bool*>(userdata);
            wgpu::ComputePipeline::Acquire(cPipeline);
            // On the wire, all callbacks get rejected immediately with once the device is deleted.
            // In native, expect success since the compilation hits the frontend cache immediately.
            // TODO(crbug.com/dawn/1122): These callbacks should be made consistent.
            EXPECT_EQ(status, wire ? WGPUCreatePipelineAsyncStatus_DeviceDestroyed
                                   : WGPUCreatePipelineAsyncStatus_Success);
        },
        &wire);
    device = nullptr;
}

// Test that the device can be dropped inside a createPipelineAsync callback which will hit the
// frontend cache
TEST_P(DeviceLifetimeTests, DroppedInsideCreatePipelineAsyncAlreadyCached) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    // Create a pipeline ahead of time so it's in the cache.
    wgpu::ComputePipeline p = device.CreateComputePipeline(&desc);

    struct Userdata {
        wgpu::Device device;
        bool done;
    };
    // Call CreateComputePipelineAsync and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), false};
    data.device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char* message,
           void* userdata) {
            wgpu::ComputePipeline::Acquire(cPipeline);
            // Success because it hits the frontend cache immediately.
            EXPECT_EQ(status, WGPUCreatePipelineAsyncStatus_Success);

            static_cast<Userdata*>(userdata)->device = nullptr;
            static_cast<Userdata*>(userdata)->done = true;
        },
        &data);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }
}

// Test that the device can be dropped while createPipelineAsync which will race with a compilation
// to add the same pipeline to the frontend cache
TEST_P(DeviceLifetimeTests, DroppedWhileCreatePipelineAsyncRaceCache) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char* message,
           void* userdata) {
            wgpu::ComputePipeline::Acquire(cPipeline);
            EXPECT_EQ(status, WGPUCreatePipelineAsyncStatus_DeviceDestroyed);
        },
        nullptr);

    // Create the same pipeline synchronously which will get added to the cache.
    wgpu::ComputePipeline p = device.CreateComputePipeline(&desc);

    device = nullptr;
}

// Test that the device can be dropped inside a createPipelineAsync callback which which will race
// with a compilation to add the same pipeline to the frontend cache
TEST_P(DeviceLifetimeTests, DroppedInsideCreatePipelineAsyncRaceCache) {
    wgpu::ComputePipelineDescriptor desc;
    desc.compute.module = utils::CreateShaderModule(device, R"(
    @compute @workgroup_size(1) fn main() {
    })");
    desc.compute.entryPoint = "main";

    struct Userdata {
        wgpu::Device device;
        bool done;
    };
    // Call CreateComputePipelineAsync and drop the device inside the callback.
    Userdata data = Userdata{std::move(device), false};
    data.device.CreateComputePipelineAsync(
        &desc,
        [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline cPipeline, const char* message,
           void* userdata) {
            wgpu::ComputePipeline::Acquire(cPipeline);
            EXPECT_EQ(status, WGPUCreatePipelineAsyncStatus_Success);

            static_cast<Userdata*>(userdata)->device = nullptr;
            static_cast<Userdata*>(userdata)->done = true;
        },
        &data);

    // Create the same pipeline synchronously which will get added to the cache.
    wgpu::ComputePipeline p = data.device.CreateComputePipeline(&desc);

    while (!data.done) {
        // WaitABit no longer can call tick since we've moved the device from the fixture into the
        // userdata.
        if (data.device) {
            data.device.Tick();
        }
        WaitABit();
    }
}

DAWN_INSTANTIATE_TEST(DeviceLifetimeTests,
                      D3D12Backend(),
                      MetalBackend(),
                      NullBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
