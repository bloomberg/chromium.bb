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

#include "dawn_wire/client/Client.h"

#include "common/Compiler.h"
#include "dawn_wire/client/Device.h"

namespace dawn_wire { namespace client {

    namespace {

        class NoopCommandSerializer final : public CommandSerializer {
          public:
            static NoopCommandSerializer* GetInstance() {
                static NoopCommandSerializer gNoopCommandSerializer;
                return &gNoopCommandSerializer;
            }

            ~NoopCommandSerializer() = default;

            size_t GetMaximumAllocationSize() const final {
                return 0;
            }
            void* GetCmdSpace(size_t size) final {
                return nullptr;
            }
            bool Flush() final {
                return false;
            }
        };

    }  // anonymous namespace

    Client::Client(CommandSerializer* serializer, MemoryTransferService* memoryTransferService)
        : ClientBase(), mSerializer(serializer), mMemoryTransferService(memoryTransferService) {
        if (mMemoryTransferService == nullptr) {
            // If a MemoryTransferService is not provided, fall back to inline memory.
            mOwnedMemoryTransferService = CreateInlineMemoryTransferService();
            mMemoryTransferService = mOwnedMemoryTransferService.get();
        }
    }

    Client::~Client() {
        DestroyAllObjects();
    }

    void Client::DestroyAllObjects() {
        while (!mDevices.empty()) {
            // Note: We don't send a DestroyObject command for the device
            // since freeing a device object is done out of band.
            DeviceAllocator().Free(static_cast<Device*>(mDevices.head()->value()));
        }
    }

    WGPUDevice Client::GetDevice() {
        if (mDevice == nullptr) {
            mDevice = DeviceAllocator().New(this)->object.get();
        }
        return reinterpret_cast<WGPUDeviceImpl*>(mDevice);
    }

    ReservedTexture Client::ReserveTexture(WGPUDevice cDevice) {
        Device* device = FromAPI(cDevice);
        auto* allocation = TextureAllocator().New(device);

        ReservedTexture result;
        result.texture = ToAPI(allocation->object.get());
        result.id = allocation->object->id;
        result.generation = allocation->generation;
        return result;
    }

    void Client::Disconnect() {
        mDisconnected = true;
        mSerializer = ChunkedCommandSerializer(NoopCommandSerializer::GetInstance());
        if (mDevice != nullptr) {
            mDevice->HandleDeviceLost("GPU connection lost");
            mDevice->CancelCallbacksForDisconnect();
        }
    }

    void Client::TrackObject(Device* device) {
        mDevices.Append(device);
    }

    bool Client::IsDisconnected() const {
        return mDisconnected;
    }

}}  // namespace dawn_wire::client
