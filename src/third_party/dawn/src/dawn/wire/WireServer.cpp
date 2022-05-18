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

#include "dawn/wire/WireServer.h"
#include "dawn/wire/server/Server.h"

namespace dawn::wire {

WireServer::WireServer(const WireServerDescriptor& descriptor)
    : mImpl(new server::Server(*descriptor.procs,
                               descriptor.serializer,
                               descriptor.memoryTransferService)) {}

WireServer::~WireServer() {
    mImpl.reset();
}

const volatile char* WireServer::HandleCommands(const volatile char* commands, size_t size) {
    return mImpl->HandleCommands(commands, size);
}

bool WireServer::InjectTexture(WGPUTexture texture,
                               uint32_t id,
                               uint32_t generation,
                               uint32_t deviceId,
                               uint32_t deviceGeneration) {
    return mImpl->InjectTexture(texture, id, generation, deviceId, deviceGeneration);
}

bool WireServer::InjectSwapChain(WGPUSwapChain swapchain,
                                 uint32_t id,
                                 uint32_t generation,
                                 uint32_t deviceId,
                                 uint32_t deviceGeneration) {
    return mImpl->InjectSwapChain(swapchain, id, generation, deviceId, deviceGeneration);
}

bool WireServer::InjectDevice(WGPUDevice device, uint32_t id, uint32_t generation) {
    return mImpl->InjectDevice(device, id, generation);
}

bool WireServer::InjectInstance(WGPUInstance instance, uint32_t id, uint32_t generation) {
    return mImpl->InjectInstance(instance, id, generation);
}

WGPUDevice WireServer::GetDevice(uint32_t id, uint32_t generation) {
    return mImpl->GetDevice(id, generation);
}

bool WireServer::IsDeviceKnown(WGPUDevice device) const {
    return mImpl->IsDeviceKnown(device);
}

namespace server {
MemoryTransferService::MemoryTransferService() = default;

MemoryTransferService::~MemoryTransferService() = default;

MemoryTransferService::ReadHandle::ReadHandle() = default;

MemoryTransferService::ReadHandle::~ReadHandle() = default;

MemoryTransferService::WriteHandle::WriteHandle() = default;

MemoryTransferService::WriteHandle::~WriteHandle() = default;

void MemoryTransferService::WriteHandle::SetTarget(void* data) {
    mTargetData = data;
}
void MemoryTransferService::WriteHandle::SetDataLength(size_t dataLength) {
    mDataLength = dataLength;
}
}  // namespace server

}  // namespace dawn::wire
