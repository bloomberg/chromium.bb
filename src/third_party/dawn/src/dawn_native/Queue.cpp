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

#include "dawn_native/Queue.h"

#include "common/Constants.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/CopyTextureForBrowserHelper.h"
#include "dawn_native/Device.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/ExternalTexture.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Texture.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

#include <cstring>

namespace dawn_native {

    namespace {

        void CopyTextureData(uint8_t* dstPointer,
                             const uint8_t* srcPointer,
                             uint32_t depth,
                             uint32_t rowsPerImage,
                             uint64_t imageAdditionalStride,
                             uint32_t actualBytesPerRow,
                             uint32_t dstBytesPerRow,
                             uint32_t srcBytesPerRow) {
            bool copyWholeLayer =
                actualBytesPerRow == dstBytesPerRow && dstBytesPerRow == srcBytesPerRow;
            bool copyWholeData = copyWholeLayer && imageAdditionalStride == 0;

            if (!copyWholeLayer) {  // copy row by row
                for (uint32_t d = 0; d < depth; ++d) {
                    for (uint32_t h = 0; h < rowsPerImage; ++h) {
                        memcpy(dstPointer, srcPointer, actualBytesPerRow);
                        dstPointer += dstBytesPerRow;
                        srcPointer += srcBytesPerRow;
                    }
                    srcPointer += imageAdditionalStride;
                }
            } else {
                uint64_t layerSize = uint64_t(rowsPerImage) * actualBytesPerRow;
                if (!copyWholeData) {  // copy layer by layer
                    for (uint32_t d = 0; d < depth; ++d) {
                        memcpy(dstPointer, srcPointer, layerSize);
                        dstPointer += layerSize;
                        srcPointer += layerSize + imageAdditionalStride;
                    }
                } else {  // do a single copy
                    memcpy(dstPointer, srcPointer, layerSize * depth);
                }
            }
        }

        ResultOrError<UploadHandle> UploadTextureDataAligningBytesPerRowAndOffset(
            DeviceBase* device,
            const void* data,
            uint32_t alignedBytesPerRow,
            uint32_t optimallyAlignedBytesPerRow,
            uint32_t alignedRowsPerImage,
            const TextureDataLayout& dataLayout,
            const TexelBlockInfo& blockInfo,
            const Extent3D& writeSizePixel) {
            uint64_t newDataSizeBytes;
            DAWN_TRY_ASSIGN(
                newDataSizeBytes,
                ComputeRequiredBytesInCopy(blockInfo, writeSizePixel, optimallyAlignedBytesPerRow,
                                           alignedRowsPerImage));

            uint64_t optimalOffsetAlignment =
                device->GetOptimalBufferToTextureCopyOffsetAlignment();
            ASSERT(IsPowerOfTwo(optimalOffsetAlignment));
            ASSERT(IsPowerOfTwo(blockInfo.byteSize));
            // We need the offset to be aligned to both optimalOffsetAlignment and blockByteSize,
            // since both of them are powers of two, we only need to align to the max value.
            uint64_t offsetAlignment =
                std::max(optimalOffsetAlignment, uint64_t(blockInfo.byteSize));

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                              newDataSizeBytes, device->GetPendingCommandSerial(),
                                              offsetAlignment));
            ASSERT(uploadHandle.mappedBuffer != nullptr);

            uint8_t* dstPointer = static_cast<uint8_t*>(uploadHandle.mappedBuffer);
            const uint8_t* srcPointer = static_cast<const uint8_t*>(data);
            srcPointer += dataLayout.offset;

            uint32_t dataRowsPerImage = dataLayout.rowsPerImage;
            if (dataRowsPerImage == 0) {
                dataRowsPerImage = writeSizePixel.height / blockInfo.height;
            }

            ASSERT(dataRowsPerImage >= alignedRowsPerImage);
            uint64_t imageAdditionalStride =
                dataLayout.bytesPerRow * (dataRowsPerImage - alignedRowsPerImage);

            CopyTextureData(dstPointer, srcPointer, writeSizePixel.depthOrArrayLayers,
                            alignedRowsPerImage, imageAdditionalStride, alignedBytesPerRow,
                            optimallyAlignedBytesPerRow, dataLayout.bytesPerRow);

            return uploadHandle;
        }

        struct SubmittedWorkDone : QueueBase::TaskInFlight {
            SubmittedWorkDone(WGPUQueueWorkDoneCallback callback, void* userdata)
                : mCallback(callback), mUserdata(userdata) {
            }
            void Finish() override {
                ASSERT(mCallback != nullptr);
                mCallback(WGPUQueueWorkDoneStatus_Success, mUserdata);
                mCallback = nullptr;
            }
            void HandleDeviceLoss() override {
                ASSERT(mCallback != nullptr);
                mCallback(WGPUQueueWorkDoneStatus_DeviceLost, mUserdata);
                mCallback = nullptr;
            }
            ~SubmittedWorkDone() override = default;

          private:
            WGPUQueueWorkDoneCallback mCallback = nullptr;
            void* mUserdata;
        };

        class ErrorQueue : public QueueBase {
          public:
            ErrorQueue(DeviceBase* device) : QueueBase(device, ObjectBase::kError) {
            }

          private:
            MaybeError SubmitImpl(uint32_t commandCount,
                                  CommandBufferBase* const* commands) override {
                UNREACHABLE();
            }
        };
    }  // namespace

    // QueueBase

    QueueBase::TaskInFlight::~TaskInFlight() {
    }

    QueueBase::QueueBase(DeviceBase* device) : ObjectBase(device) {
    }

    QueueBase::QueueBase(DeviceBase* device, ObjectBase::ErrorTag tag) : ObjectBase(device, tag) {
    }

    QueueBase::~QueueBase() {
        ASSERT(mTasksInFlight.Empty());
    }

    // static
    QueueBase* QueueBase::MakeError(DeviceBase* device) {
        return new ErrorQueue(device);
    }

    void QueueBase::APISubmit(uint32_t commandCount, CommandBufferBase* const* commands) {
        SubmitInternal(commandCount, commands);

        for (uint32_t i = 0; i < commandCount; ++i) {
            commands[i]->Destroy();
        }
    }

    void QueueBase::APIOnSubmittedWorkDone(uint64_t signalValue,
                                           WGPUQueueWorkDoneCallback callback,
                                           void* userdata) {
        // The error status depends on the type of error so we let the validation function choose it
        WGPUQueueWorkDoneStatus status;
        if (GetDevice()->ConsumedError(ValidateOnSubmittedWorkDone(signalValue, &status))) {
            callback(status, userdata);
            return;
        }

        std::unique_ptr<SubmittedWorkDone> task =
            std::make_unique<SubmittedWorkDone>(callback, userdata);

        // Technically we only need to wait for previously submitted work but OnSubmittedWorkDone is
        // also used to make sure ALL queue work is finished in tests, so we also wait for pending
        // commands (this is non-observable outside of tests so it's ok to do deviate a bit from the
        // spec).
        TrackTask(std::move(task), GetDevice()->GetPendingCommandSerial());
    }

    void QueueBase::TrackTask(std::unique_ptr<TaskInFlight> task, ExecutionSerial serial) {
        mTasksInFlight.Enqueue(std::move(task), serial);
        GetDevice()->AddFutureSerial(serial);
    }

    void QueueBase::Tick(ExecutionSerial finishedSerial) {
        // If a user calls Queue::Submit inside a task, for example in a Buffer::MapAsync callback,
        // then the device will be ticked, which in turns ticks the queue, causing reentrance here.
        // To prevent the reentrant call from invalidating mTasksInFlight while in use by the first
        // call, we remove the tasks to finish from the queue, update mTasksInFlight, then run the
        // callbacks.
        std::vector<std::unique_ptr<TaskInFlight>> tasks;
        for (auto& task : mTasksInFlight.IterateUpTo(finishedSerial)) {
            tasks.push_back(std::move(task));
        }
        mTasksInFlight.ClearUpTo(finishedSerial);

        for (auto& task : tasks) {
            task->Finish();
        }
    }

    void QueueBase::HandleDeviceLoss() {
        for (auto& task : mTasksInFlight.IterateAll()) {
            task->HandleDeviceLoss();
        }
        mTasksInFlight.Clear();
    }

    void QueueBase::APIWriteBuffer(BufferBase* buffer,
                                   uint64_t bufferOffset,
                                   const void* data,
                                   size_t size) {
        GetDevice()->ConsumedError(WriteBuffer(buffer, bufferOffset, data, size));
    }

    MaybeError QueueBase::WriteBuffer(BufferBase* buffer,
                                      uint64_t bufferOffset,
                                      const void* data,
                                      size_t size) {
        DAWN_TRY(ValidateWriteBuffer(buffer, bufferOffset, size));
        return WriteBufferImpl(buffer, bufferOffset, data, size);
    }

    MaybeError QueueBase::WriteBufferImpl(BufferBase* buffer,
                                          uint64_t bufferOffset,
                                          const void* data,
                                          size_t size) {
        if (size == 0) {
            return {};
        }

        DeviceBase* device = GetDevice();

        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                          size, device->GetPendingCommandSerial(),
                                          kCopyBufferToBufferOffsetAlignment));
        ASSERT(uploadHandle.mappedBuffer != nullptr);

        memcpy(uploadHandle.mappedBuffer, data, size);

        device->AddFutureSerial(device->GetPendingCommandSerial());

        return device->CopyFromStagingToBuffer(uploadHandle.stagingBuffer, uploadHandle.startOffset,
                                               buffer, bufferOffset, size);
    }

    void QueueBase::APIWriteTexture(const ImageCopyTexture* destination,
                                    const void* data,
                                    size_t dataSize,
                                    const TextureDataLayout* dataLayout,
                                    const Extent3D* writeSize) {
        GetDevice()->ConsumedError(
            WriteTextureInternal(destination, data, dataSize, *dataLayout, writeSize));
    }

    MaybeError QueueBase::WriteTextureInternal(const ImageCopyTexture* destination,
                                               const void* data,
                                               size_t dataSize,
                                               const TextureDataLayout& dataLayout,
                                               const Extent3D* writeSize) {
        DAWN_TRY(ValidateWriteTexture(destination, dataSize, dataLayout, writeSize));

        if (writeSize->width == 0 || writeSize->height == 0 || writeSize->depthOrArrayLayers == 0) {
            return {};
        }

        const TexelBlockInfo& blockInfo =
            destination->texture->GetFormat().GetAspectInfo(destination->aspect).block;
        TextureDataLayout layout = dataLayout;
        ApplyDefaultTextureDataLayoutOptions(&layout, blockInfo, *writeSize);
        return WriteTextureImpl(*destination, data, layout, *writeSize);
    }

    MaybeError QueueBase::WriteTextureImpl(const ImageCopyTexture& destination,
                                           const void* data,
                                           const TextureDataLayout& dataLayout,
                                           const Extent3D& writeSizePixel) {
        const TexelBlockInfo& blockInfo =
            destination.texture->GetFormat().GetAspectInfo(destination.aspect).block;

        // We are only copying the part of the data that will appear in the texture.
        // Note that validating texture copy range ensures that writeSizePixel->width and
        // writeSizePixel->height are multiples of blockWidth and blockHeight respectively.
        ASSERT(writeSizePixel.width % blockInfo.width == 0);
        ASSERT(writeSizePixel.height % blockInfo.height == 0);
        uint32_t alignedBytesPerRow = writeSizePixel.width / blockInfo.width * blockInfo.byteSize;
        uint32_t alignedRowsPerImage = writeSizePixel.height / blockInfo.height;

        uint32_t optimalBytesPerRowAlignment = GetDevice()->GetOptimalBytesPerRowAlignment();
        uint32_t optimallyAlignedBytesPerRow =
            Align(alignedBytesPerRow, optimalBytesPerRowAlignment);

        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(uploadHandle,
                        UploadTextureDataAligningBytesPerRowAndOffset(
                            GetDevice(), data, alignedBytesPerRow, optimallyAlignedBytesPerRow,
                            alignedRowsPerImage, dataLayout, blockInfo, writeSizePixel));

        TextureDataLayout passDataLayout = dataLayout;
        passDataLayout.offset = uploadHandle.startOffset;
        passDataLayout.bytesPerRow = optimallyAlignedBytesPerRow;
        passDataLayout.rowsPerImage = alignedRowsPerImage;

        TextureCopy textureCopy;
        textureCopy.texture = destination.texture;
        textureCopy.mipLevel = destination.mipLevel;
        textureCopy.origin = destination.origin;
        textureCopy.aspect = ConvertAspect(destination.texture->GetFormat(), destination.aspect);

        DeviceBase* device = GetDevice();

        device->AddFutureSerial(device->GetPendingCommandSerial());

        return device->CopyFromStagingToTexture(uploadHandle.stagingBuffer, passDataLayout,
                                                &textureCopy, writeSizePixel);
    }

    void QueueBase::APICopyTextureForBrowser(const ImageCopyTexture* source,
                                             const ImageCopyTexture* destination,
                                             const Extent3D* copySize,
                                             const CopyTextureForBrowserOptions* options) {
        GetDevice()->ConsumedError(
            CopyTextureForBrowserInternal(source, destination, copySize, options));
    }

    MaybeError QueueBase::CopyTextureForBrowserInternal(
        const ImageCopyTexture* source,
        const ImageCopyTexture* destination,
        const Extent3D* copySize,
        const CopyTextureForBrowserOptions* options) {
        if (GetDevice()->IsValidationEnabled()) {
            DAWN_TRY(
                ValidateCopyTextureForBrowser(GetDevice(), source, destination, copySize, options));
        }

        return DoCopyTextureForBrowser(GetDevice(), source, destination, copySize, options);
    }

    MaybeError QueueBase::ValidateSubmit(uint32_t commandCount,
                                         CommandBufferBase* const* commands) const {
        TRACE_EVENT0(GetDevice()->GetPlatform(), Validation, "Queue::ValidateSubmit");
        DAWN_TRY(GetDevice()->ValidateObject(this));

        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(GetDevice()->ValidateObject(commands[i]));
            DAWN_TRY(commands[i]->ValidateCanUseInSubmitNow());

            const CommandBufferResourceUsage& usages = commands[i]->GetResourceUsages();

            for (const SyncScopeResourceUsage& scope : usages.renderPasses) {
                for (const BufferBase* buffer : scope.buffers) {
                    DAWN_TRY(buffer->ValidateCanUseOnQueueNow());
                }

                for (const TextureBase* texture : scope.textures) {
                    DAWN_TRY(texture->ValidateCanUseInSubmitNow());
                }

                for (const ExternalTextureBase* externalTexture : scope.externalTextures) {
                    DAWN_TRY(externalTexture->ValidateCanUseInSubmitNow());
                }
            }

            for (const ComputePassResourceUsage& pass : usages.computePasses) {
                for (const BufferBase* buffer : pass.referencedBuffers) {
                    DAWN_TRY(buffer->ValidateCanUseOnQueueNow());
                }
                for (const TextureBase* texture : pass.referencedTextures) {
                    DAWN_TRY(texture->ValidateCanUseInSubmitNow());
                }
                for (const ExternalTextureBase* externalTexture : pass.referencedExternalTextures) {
                    DAWN_TRY(externalTexture->ValidateCanUseInSubmitNow());
                }
            }

            for (const BufferBase* buffer : usages.topLevelBuffers) {
                DAWN_TRY(buffer->ValidateCanUseOnQueueNow());
            }
            for (const TextureBase* texture : usages.topLevelTextures) {
                DAWN_TRY(texture->ValidateCanUseInSubmitNow());
            }
            for (const QuerySetBase* querySet : usages.usedQuerySets) {
                DAWN_TRY(querySet->ValidateCanUseInSubmitNow());
            }
        }

        return {};
    }

    MaybeError QueueBase::ValidateOnSubmittedWorkDone(uint64_t signalValue,
                                                      WGPUQueueWorkDoneStatus* status) const {
        *status = WGPUQueueWorkDoneStatus_DeviceLost;
        DAWN_TRY(GetDevice()->ValidateIsAlive());

        *status = WGPUQueueWorkDoneStatus_Error;
        DAWN_TRY(GetDevice()->ValidateObject(this));

        if (signalValue != 0) {
            return DAWN_VALIDATION_ERROR("SignalValue must currently be 0.");
        }

        return {};
    }

    MaybeError QueueBase::ValidateWriteBuffer(const BufferBase* buffer,
                                              uint64_t bufferOffset,
                                              size_t size) const {
        DAWN_TRY(GetDevice()->ValidateIsAlive());
        DAWN_TRY(GetDevice()->ValidateObject(this));
        DAWN_TRY(GetDevice()->ValidateObject(buffer));

        if (bufferOffset % 4 != 0) {
            return DAWN_VALIDATION_ERROR("Queue::WriteBuffer bufferOffset must be a multiple of 4");
        }
        if (size % 4 != 0) {
            return DAWN_VALIDATION_ERROR("Queue::WriteBuffer size must be a multiple of 4");
        }

        uint64_t bufferSize = buffer->GetSize();
        if (bufferOffset > bufferSize || size > (bufferSize - bufferOffset)) {
            return DAWN_VALIDATION_ERROR("Queue::WriteBuffer out of range");
        }

        if (!(buffer->GetUsage() & wgpu::BufferUsage::CopyDst)) {
            return DAWN_VALIDATION_ERROR("Buffer needs the CopyDst usage bit");
        }

        DAWN_TRY(buffer->ValidateCanUseOnQueueNow());

        return {};
    }

    MaybeError QueueBase::ValidateWriteTexture(const ImageCopyTexture* destination,
                                               size_t dataSize,
                                               const TextureDataLayout& dataLayout,
                                               const Extent3D* writeSize) const {
        DAWN_TRY(GetDevice()->ValidateIsAlive());
        DAWN_TRY(GetDevice()->ValidateObject(this));
        DAWN_TRY(GetDevice()->ValidateObject(destination->texture));

        DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *destination, *writeSize));

        if (dataLayout.offset > dataSize) {
            return DAWN_VALIDATION_ERROR("Queue::WriteTexture out of range");
        }

        if (!(destination->texture->GetUsage() & wgpu::TextureUsage::CopyDst)) {
            return DAWN_VALIDATION_ERROR("Texture needs the CopyDst usage bit");
        }

        if (destination->texture->GetSampleCount() > 1) {
            return DAWN_VALIDATION_ERROR("The sample count of textures must be 1");
        }

        DAWN_TRY(ValidateLinearToDepthStencilCopyRestrictions(*destination));
        // We validate texture copy range before validating linear texture data,
        // because in the latter we divide copyExtent.width by blockWidth and
        // copyExtent.height by blockHeight while the divisibility conditions are
        // checked in validating texture copy range.
        DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *destination, *writeSize));

        const TexelBlockInfo& blockInfo =
            destination->texture->GetFormat().GetAspectInfo(destination->aspect).block;

        DAWN_TRY(ValidateLinearTextureData(dataLayout, dataSize, blockInfo, *writeSize));

        DAWN_TRY(destination->texture->ValidateCanUseInSubmitNow());

        return {};
    }

    void QueueBase::SubmitInternal(uint32_t commandCount, CommandBufferBase* const* commands) {
        DeviceBase* device = GetDevice();
        if (device->ConsumedError(device->ValidateIsAlive())) {
            // If device is lost, don't let any commands be submitted
            return;
        }

        TRACE_EVENT0(device->GetPlatform(), General, "Queue::Submit");
        if (device->IsValidationEnabled() &&
            device->ConsumedError(ValidateSubmit(commandCount, commands))) {
            return;
        }
        ASSERT(!IsError());

        if (device->ConsumedError(SubmitImpl(commandCount, commands))) {
            return;
        }
    }

}  // namespace dawn_native
