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

#ifndef SRC_DAWN_NATIVE_COMMANDENCODER_H_
#define SRC_DAWN_NATIVE_COMMANDENCODER_H_

#include <set>
#include <string>

#include "dawn/native/dawn_platform.h"

#include "dawn/native/EncodingContext.h"
#include "dawn/native/Error.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/native/PassResourceUsage.h"

namespace dawn::native {

enum class UsageValidationMode;

MaybeError ValidateCommandEncoderDescriptor(const DeviceBase* device,
                                            const CommandEncoderDescriptor* descriptor);

class CommandEncoder final : public ApiObjectBase {
  public:
    static Ref<CommandEncoder> Create(DeviceBase* device,
                                      const CommandEncoderDescriptor* descriptor);
    static CommandEncoder* MakeError(DeviceBase* device);

    ObjectType GetType() const override;

    CommandIterator AcquireCommands();
    CommandBufferResourceUsage AcquireResourceUsages();

    void TrackUsedQuerySet(QuerySetBase* querySet);
    void TrackQueryAvailability(QuerySetBase* querySet, uint32_t queryIndex);

    // Dawn API
    ComputePassEncoder* APIBeginComputePass(const ComputePassDescriptor* descriptor);
    RenderPassEncoder* APIBeginRenderPass(const RenderPassDescriptor* descriptor);

    void APICopyBufferToBuffer(BufferBase* source,
                               uint64_t sourceOffset,
                               BufferBase* destination,
                               uint64_t destinationOffset,
                               uint64_t size);
    void APICopyBufferToTexture(const ImageCopyBuffer* source,
                                const ImageCopyTexture* destination,
                                const Extent3D* copySize);
    void APICopyTextureToBuffer(const ImageCopyTexture* source,
                                const ImageCopyBuffer* destination,
                                const Extent3D* copySize);
    void APICopyTextureToTexture(const ImageCopyTexture* source,
                                 const ImageCopyTexture* destination,
                                 const Extent3D* copySize);
    void APICopyTextureToTextureInternal(const ImageCopyTexture* source,
                                         const ImageCopyTexture* destination,
                                         const Extent3D* copySize);
    void APIClearBuffer(BufferBase* destination, uint64_t destinationOffset, uint64_t size);

    void APIInjectValidationError(const char* message);
    void APIInsertDebugMarker(const char* groupLabel);
    void APIPopDebugGroup();
    void APIPushDebugGroup(const char* groupLabel);

    void APIResolveQuerySet(QuerySetBase* querySet,
                            uint32_t firstQuery,
                            uint32_t queryCount,
                            BufferBase* destination,
                            uint64_t destinationOffset);
    void APIWriteBuffer(BufferBase* buffer,
                        uint64_t bufferOffset,
                        const uint8_t* data,
                        uint64_t size);
    void APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex);

    CommandBufferBase* APIFinish(const CommandBufferDescriptor* descriptor = nullptr);

    Ref<ComputePassEncoder> BeginComputePass(const ComputePassDescriptor* descriptor = nullptr);
    Ref<RenderPassEncoder> BeginRenderPass(const RenderPassDescriptor* descriptor);
    ResultOrError<Ref<CommandBufferBase>> Finish(
        const CommandBufferDescriptor* descriptor = nullptr);

  private:
    CommandEncoder(DeviceBase* device, const CommandEncoderDescriptor* descriptor);
    CommandEncoder(DeviceBase* device, ObjectBase::ErrorTag tag);

    void DestroyImpl() override;

    // Helper to be able to implement both APICopyTextureToTexture and
    // APICopyTextureToTextureInternal. The only difference between both
    // copies, is that the Internal one will also check internal usage.
    template <bool Internal>
    void APICopyTextureToTextureHelper(const ImageCopyTexture* source,
                                       const ImageCopyTexture* destination,
                                       const Extent3D* copySize);

    MaybeError ValidateFinish() const;

    EncodingContext mEncodingContext;
    std::set<BufferBase*> mTopLevelBuffers;
    std::set<TextureBase*> mTopLevelTextures;
    std::set<QuerySetBase*> mUsedQuerySets;

    uint64_t mDebugGroupStackSize = 0;

    UsageValidationMode mUsageValidationMode;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_COMMANDENCODER_H_
