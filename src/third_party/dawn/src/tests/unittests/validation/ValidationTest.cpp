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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Assert.h"
#include "dawn/dawn_proc.h"
#include "dawn/webgpu.h"
#include "dawn_native/NullBackend.h"

void ValidationTest::SetUp() {
    instance = std::make_unique<dawn_native::Instance>();
    instance->DiscoverDefaultAdapters();

    std::vector<dawn_native::Adapter> adapters = instance->GetAdapters();

    // Validation tests run against the null backend, find the corresponding adapter
    bool foundNullAdapter = false;
    for (auto& currentAdapter : adapters) {
        wgpu::AdapterProperties adapterProperties;
        currentAdapter.GetProperties(&adapterProperties);

        if (adapterProperties.backendType == wgpu::BackendType::Null) {
            adapter = currentAdapter;
            foundNullAdapter = true;
            break;
        }
    }

    ASSERT(foundNullAdapter);

    dawnProcSetProcs(&dawn_native::GetProcs());

    device = CreateTestDevice();
    device.SetUncapturedErrorCallback(ValidationTest::OnDeviceError, this);
}

ValidationTest::~ValidationTest() {
    // We need to destroy Dawn objects before setting the procs to null otherwise the dawn*Release
    // will call a nullptr
    device = wgpu::Device();
    dawnProcSetProcs(nullptr);
}

void ValidationTest::TearDown() {
    ASSERT_FALSE(mExpectError);

    if (device) {
        EXPECT_EQ(mLastWarningCount,
                  dawn_native::GetDeprecationWarningCountForTesting(device.Get()));
    }
}

void ValidationTest::StartExpectDeviceError() {
    mExpectError = true;
    mError = false;
}
bool ValidationTest::EndExpectDeviceError() {
    mExpectError = false;
    return mError;
}
std::string ValidationTest::GetLastDeviceErrorMessage() const {
    return mDeviceErrorMessage;
}

void ValidationTest::WaitForAllOperations(const wgpu::Device& device) const {
    wgpu::Queue queue = device.GetDefaultQueue();
    wgpu::Fence fence = queue.CreateFence();

    // Force the currently submitted operations to completed.
    queue.Signal(fence, 1);
    while (fence.GetCompletedValue() < 1) {
        device.Tick();
    }

    // TODO(cwallez@chromium.org): It's not clear why we need this additional tick. Investigate it
    // once WebGPU has defined the ordering of callbacks firing.
    device.Tick();
}

bool ValidationTest::HasWGSL() const {
#ifdef DAWN_ENABLE_WGSL
    return true;
#else
    return false;
#endif
}

wgpu::Device ValidationTest::CreateTestDevice() {
    return wgpu::Device::Acquire(adapter.CreateDevice());
}

// static
void ValidationTest::OnDeviceError(WGPUErrorType type, const char* message, void* userdata) {
    ASSERT(type != WGPUErrorType_NoError);
    auto self = static_cast<ValidationTest*>(userdata);
    self->mDeviceErrorMessage = message;

    ASSERT_TRUE(self->mExpectError) << "Got unexpected device error: " << message;
    ASSERT_FALSE(self->mError) << "Got two errors in expect block";
    self->mError = true;
}

ValidationTest::DummyRenderPass::DummyRenderPass(const wgpu::Device& device)
    : attachmentFormat(wgpu::TextureFormat::RGBA8Unorm), width(400), height(400) {
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depth = 1;
    descriptor.sampleCount = 1;
    descriptor.format = attachmentFormat;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    attachment = device.CreateTexture(&descriptor);

    wgpu::TextureView view = attachment.CreateView();
    mColorAttachment.attachment = view;
    mColorAttachment.resolveTarget = nullptr;
    mColorAttachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    mColorAttachment.loadOp = wgpu::LoadOp::Clear;
    mColorAttachment.storeOp = wgpu::StoreOp::Store;

    colorAttachmentCount = 1;
    colorAttachments = &mColorAttachment;
    depthStencilAttachment = nullptr;
}
