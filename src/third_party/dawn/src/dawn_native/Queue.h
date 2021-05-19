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

#ifndef DAWNNATIVE_QUEUE_H_
#define DAWNNATIVE_QUEUE_H_

#include "common/SerialQueue.h"
#include "dawn_native/Error.h"
#include "dawn_native/Forward.h"
#include "dawn_native/IntegerTypes.h"
#include "dawn_native/ObjectBase.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    class QueueBase : public ObjectBase {
      public:
        struct TaskInFlight {
            virtual ~TaskInFlight();
            virtual void Finish() = 0;
            virtual void HandleDeviceLoss() = 0;
        };

        static QueueBase* MakeError(DeviceBase* device);
        ~QueueBase() override;

        // Dawn API
        void APISubmit(uint32_t commandCount, CommandBufferBase* const* commands);
        void APISignal(Fence* fence, uint64_t signalValue);
        Fence* APICreateFence(const FenceDescriptor* descriptor);
        void APIOnSubmittedWorkDone(uint64_t signalValue,
                                    WGPUQueueWorkDoneCallback callback,
                                    void* userdata);
        void APIWriteBuffer(BufferBase* buffer,
                            uint64_t bufferOffset,
                            const void* data,
                            size_t size);
        void APIWriteTexture(const ImageCopyTexture* destination,
                             const void* data,
                             size_t dataSize,
                             const TextureDataLayout* dataLayout,
                             const Extent3D* writeSize);
        void APICopyTextureForBrowser(const ImageCopyTexture* source,
                                      const ImageCopyTexture* destination,
                                      const Extent3D* copySize,
                                      const CopyTextureForBrowserOptions* options);

        MaybeError WriteBuffer(BufferBase* buffer,
                               uint64_t bufferOffset,
                               const void* data,
                               size_t size);
        void TrackTask(std::unique_ptr<TaskInFlight> task, ExecutionSerial serial);
        void Tick(ExecutionSerial finishedSerial);
        void HandleDeviceLoss();

      protected:
        QueueBase(DeviceBase* device);
        QueueBase(DeviceBase* device, ObjectBase::ErrorTag tag);

      private:
        MaybeError WriteTextureInternal(const ImageCopyTexture* destination,
                                        const void* data,
                                        size_t dataSize,
                                        const TextureDataLayout* dataLayout,
                                        const Extent3D* writeSize);
        MaybeError CopyTextureForBrowserInternal(const ImageCopyTexture* source,
                                                 const ImageCopyTexture* destination,
                                                 const Extent3D* copySize,
                                                 const CopyTextureForBrowserOptions* options);

        virtual MaybeError SubmitImpl(uint32_t commandCount,
                                      CommandBufferBase* const* commands) = 0;
        virtual MaybeError WriteBufferImpl(BufferBase* buffer,
                                           uint64_t bufferOffset,
                                           const void* data,
                                           size_t size);
        virtual MaybeError WriteTextureImpl(const ImageCopyTexture& destination,
                                            const void* data,
                                            const TextureDataLayout& dataLayout,
                                            const Extent3D& writeSize);

        MaybeError ValidateSubmit(uint32_t commandCount, CommandBufferBase* const* commands) const;
        MaybeError ValidateSignal(const Fence* fence, FenceAPISerial signalValue) const;
        MaybeError ValidateOnSubmittedWorkDone(uint64_t signalValue,
                                               WGPUQueueWorkDoneStatus* status) const;
        MaybeError ValidateCreateFence(const FenceDescriptor* descriptor) const;
        MaybeError ValidateWriteBuffer(const BufferBase* buffer,
                                       uint64_t bufferOffset,
                                       size_t size) const;
        MaybeError ValidateWriteTexture(const ImageCopyTexture* destination,
                                        size_t dataSize,
                                        const TextureDataLayout* dataLayout,
                                        const Extent3D* writeSize) const;

        void SubmitInternal(uint32_t commandCount, CommandBufferBase* const* commands);

        SerialQueue<ExecutionSerial, std::unique_ptr<TaskInFlight>> mTasksInFlight;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_QUEUE_H_
