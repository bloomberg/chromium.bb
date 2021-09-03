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

#include "common/Assert.h"
#include "dawn_wire/BufferConsumer_impl.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/server/Server.h"

#include <memory>

namespace dawn_wire { namespace server {

    bool Server::PreHandleBufferUnmap(const BufferUnmapCmd& cmd) {
        auto* buffer = BufferObjects().Get(cmd.selfId);
        DAWN_ASSERT(buffer != nullptr);

        // The buffer was unmapped. Clear the Read/WriteHandle.
        buffer->readHandle = nullptr;
        buffer->writeHandle = nullptr;
        buffer->mapWriteState = BufferMapWriteState::Unmapped;

        return true;
    }

    bool Server::PreHandleBufferDestroy(const BufferDestroyCmd& cmd) {
        // Destroying a buffer does an implicit unmapping.
        auto* buffer = BufferObjects().Get(cmd.selfId);
        DAWN_ASSERT(buffer != nullptr);

        // The buffer was destroyed. Clear the Read/WriteHandle.
        buffer->readHandle = nullptr;
        buffer->writeHandle = nullptr;
        buffer->mapWriteState = BufferMapWriteState::Unmapped;

        return true;
    }

    bool Server::DoBufferMapAsync(ObjectId bufferId,
                                  uint32_t requestSerial,
                                  WGPUMapModeFlags mode,
                                  uint64_t offset64,
                                  uint64_t size64,
                                  uint64_t handleCreateInfoLength,
                                  const uint8_t* handleCreateInfo) {
        // These requests are just forwarded to the buffer, with userdata containing what the
        // client will require in the return command.

        // The null object isn't valid as `self`
        if (bufferId == 0) {
            return false;
        }

        auto* buffer = BufferObjects().Get(bufferId);
        if (buffer == nullptr) {
            return false;
        }

        // The server only knows how to deal with write XOR read. Validate that.
        bool isReadMode = mode & WGPUMapMode_Read;
        bool isWriteMode = mode & WGPUMapMode_Write;
        if (!(isReadMode ^ isWriteMode)) {
            return false;
        }

        std::unique_ptr<MapUserdata> userdata = MakeUserdata<MapUserdata>();
        userdata->buffer = ObjectHandle{bufferId, buffer->generation};
        userdata->bufferObj = buffer->handle;
        userdata->requestSerial = requestSerial;
        userdata->mode = mode;

        if (offset64 > std::numeric_limits<size_t>::max() ||
            size64 > std::numeric_limits<size_t>::max() ||
            handleCreateInfoLength > std::numeric_limits<size_t>::max()) {
            OnBufferMapAsyncCallback(WGPUBufferMapAsyncStatus_Error, userdata.get());
            return true;
        }

        size_t offset = static_cast<size_t>(offset64);
        size_t size = static_cast<size_t>(size64);

        userdata->offset = offset;
        userdata->size = size;

        // The handle will point to the mapped memory or staging memory for the mapping.
        // Store it on the map request.
        if (isWriteMode) {
            // Deserialize metadata produced from the client to create a companion server handle.
            MemoryTransferService::WriteHandle* writeHandle = nullptr;
            if (!mMemoryTransferService->DeserializeWriteHandle(
                    handleCreateInfo, static_cast<size_t>(handleCreateInfoLength), &writeHandle)) {
                return false;
            }
            ASSERT(writeHandle != nullptr);

            userdata->writeHandle =
                std::unique_ptr<MemoryTransferService::WriteHandle>(writeHandle);
        } else {
            ASSERT(isReadMode);
            // Deserialize metadata produced from the client to create a companion server handle.
            MemoryTransferService::ReadHandle* readHandle = nullptr;
            if (!mMemoryTransferService->DeserializeReadHandle(
                    handleCreateInfo, static_cast<size_t>(handleCreateInfoLength), &readHandle)) {
                return false;
            }
            ASSERT(readHandle != nullptr);

            userdata->readHandle = std::unique_ptr<MemoryTransferService::ReadHandle>(readHandle);
        }

        mProcs.bufferMapAsync(
            buffer->handle, mode, offset, size,
            ForwardToServer<decltype(
                &Server::OnBufferMapAsyncCallback)>::Func<&Server::OnBufferMapAsyncCallback>(),
            userdata.release());

        return true;
    }

    bool Server::DoDeviceCreateBuffer(ObjectId deviceId,
                                      const WGPUBufferDescriptor* descriptor,
                                      ObjectHandle bufferResult,
                                      uint64_t handleCreateInfoLength,
                                      const uint8_t* handleCreateInfo) {
        auto* device = DeviceObjects().Get(deviceId);
        if (device == nullptr) {
            return false;
        }

        // Create and register the buffer object.
        auto* resultData = BufferObjects().Allocate(bufferResult.id);
        if (resultData == nullptr) {
            return false;
        }
        resultData->generation = bufferResult.generation;
        resultData->handle = mProcs.deviceCreateBuffer(device->handle, descriptor);
        resultData->deviceInfo = device->info.get();
        if (!TrackDeviceChild(resultData->deviceInfo, ObjectType::Buffer, bufferResult.id)) {
            return false;
        }

        // If the buffer isn't mapped at creation, we are done.
        if (!descriptor->mappedAtCreation) {
            return handleCreateInfoLength == 0;
        }

        // This is the size of data deserialized from the command stream to create the write handle,
        // which must be CPU-addressable.
        if (handleCreateInfoLength > std::numeric_limits<size_t>::max()) {
            return false;
        }

        void* mapping = mProcs.bufferGetMappedRange(resultData->handle, 0, descriptor->size);
        if (mapping == nullptr) {
            // A zero mapping is used to indicate an allocation error of an error buffer. This is a
            // valid case and isn't fatal. Remember the buffer is an error so as to skip subsequent
            // mapping operations.
            resultData->mapWriteState = BufferMapWriteState::MapError;
            return true;
        }

        // Deserialize metadata produced from the client to create a companion server handle.
        MemoryTransferService::WriteHandle* writeHandle = nullptr;
        if (!mMemoryTransferService->DeserializeWriteHandle(
                handleCreateInfo, static_cast<size_t>(handleCreateInfoLength), &writeHandle)) {
            return false;
        }

        // Set the target of the WriteHandle to the mapped GPU memory.
        ASSERT(writeHandle != nullptr);
        writeHandle->SetTarget(mapping, descriptor->size);

        resultData->mapWriteState = BufferMapWriteState::Mapped;
        resultData->writeHandle.reset(writeHandle);

        return true;
    }

    bool Server::DoBufferUpdateMappedData(ObjectId bufferId,
                                          uint64_t writeFlushInfoLength,
                                          const uint8_t* writeFlushInfo) {
        // The null object isn't valid as `self`
        if (bufferId == 0) {
            return false;
        }

        if (writeFlushInfoLength > std::numeric_limits<size_t>::max()) {
            return false;
        }

        auto* buffer = BufferObjects().Get(bufferId);
        if (buffer == nullptr) {
            return false;
        }
        switch (buffer->mapWriteState) {
            case BufferMapWriteState::Unmapped:
                return false;
            case BufferMapWriteState::MapError:
                // The buffer is mapped but there was an error allocating mapped data.
                // Do not perform the memcpy.
                return true;
            case BufferMapWriteState::Mapped:
                break;
        }
        if (!buffer->writeHandle) {
            // This check is performed after the check for the MapError state. It is permissible
            // to Unmap and attempt to update mapped data of an error buffer.
            return false;
        }

        // Deserialize the flush info and flush updated data from the handle into the target
        // of the handle. The target is set via WriteHandle::SetTarget.
        return buffer->writeHandle->DeserializeFlush(writeFlushInfo,
                                                     static_cast<size_t>(writeFlushInfoLength));
    }

    void Server::OnBufferMapAsyncCallback(WGPUBufferMapAsyncStatus status, MapUserdata* data) {
        // Skip sending the callback if the buffer has already been destroyed.
        auto* bufferData = BufferObjects().Get(data->buffer.id);
        if (bufferData == nullptr || bufferData->generation != data->buffer.generation) {
            return;
        }

        bool isRead = data->mode & WGPUMapMode_Read;
        bool isSuccess = status == WGPUBufferMapAsyncStatus_Success;

        ReturnBufferMapAsyncCallbackCmd cmd;
        cmd.buffer = data->buffer;
        cmd.requestSerial = data->requestSerial;
        cmd.status = status;
        cmd.readInitialDataInfoLength = 0;
        cmd.readInitialDataInfo = nullptr;

        const void* readData = nullptr;
        if (isSuccess && isRead) {
            // Get the serialization size of the message to initialize ReadHandle data.
            readData = mProcs.bufferGetConstMappedRange(data->bufferObj, data->offset, data->size);
            cmd.readInitialDataInfoLength =
                data->readHandle->SerializeInitialDataSize(readData, data->size);
        }

        SerializeCommand(cmd, cmd.readInitialDataInfoLength, [&](SerializeBuffer* serializeBuffer) {
            if (isSuccess) {
                if (isRead) {
                    char* readHandleBuffer;
                    WIRE_TRY(
                        serializeBuffer->NextN(cmd.readInitialDataInfoLength, &readHandleBuffer));

                    // Serialize the initialization message into the space after the command.
                    data->readHandle->SerializeInitialData(readData, data->size, readHandleBuffer);
                    // The in-flight map request returned successfully.
                    // Move the ReadHandle so it is owned by the buffer.
                    bufferData->readHandle = std::move(data->readHandle);
                } else {
                    // The in-flight map request returned successfully.
                    // Move the WriteHandle so it is owned by the buffer.
                    bufferData->writeHandle = std::move(data->writeHandle);
                    bufferData->mapWriteState = BufferMapWriteState::Mapped;
                    // Set the target of the WriteHandle to the mapped buffer data.
                    bufferData->writeHandle->SetTarget(
                        mProcs.bufferGetMappedRange(data->bufferObj, data->offset, data->size),
                        data->size);
                }
            }
            return WireResult::Success;
        });
    }

}}  // namespace dawn_wire::server
