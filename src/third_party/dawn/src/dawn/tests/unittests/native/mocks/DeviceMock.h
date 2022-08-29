// Copyright 2021 The Dawn Authors
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

#ifndef SRC_DAWN_TESTS_UNITTESTS_NATIVE_MOCKS_DEVICEMOCK_H_
#define SRC_DAWN_TESTS_UNITTESTS_NATIVE_MOCKS_DEVICEMOCK_H_

#include <memory>

#include "dawn/native/Device.h"
#include "dawn/native/RenderPipeline.h"
#include "gmock/gmock.h"

namespace dawn::native {

class DeviceMock : public DeviceBase {
  public:
    // Exposes some protected functions for testing purposes.
    using DeviceBase::DestroyObjects;
    using DeviceBase::SetToggle;

    MOCK_METHOD(ResultOrError<Ref<CommandBufferBase>>,
                CreateCommandBuffer,
                (CommandEncoder*, const CommandBufferDescriptor*),
                (override));

    MOCK_METHOD(ResultOrError<std::unique_ptr<StagingBufferBase>>,
                CreateStagingBuffer,
                (size_t),
                (override));
    MOCK_METHOD(MaybeError,
                CopyFromStagingToBuffer,
                (StagingBufferBase*, uint64_t, BufferBase*, uint64_t, uint64_t),
                (override));
    MOCK_METHOD(MaybeError,
                CopyFromStagingToTexture,
                (const StagingBufferBase*, const TextureDataLayout&, TextureCopy*, const Extent3D&),
                (override));

    MOCK_METHOD(uint32_t, GetOptimalBytesPerRowAlignment, (), (const, override));
    MOCK_METHOD(uint64_t, GetOptimalBufferToTextureCopyOffsetAlignment, (), (const, override));

    MOCK_METHOD(float, GetTimestampPeriodInNS, (), (const, override));

    MOCK_METHOD(ResultOrError<Ref<BindGroupBase>>,
                CreateBindGroupImpl,
                (const BindGroupDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<BindGroupLayoutBase>>,
                CreateBindGroupLayoutImpl,
                (const BindGroupLayoutDescriptor*, PipelineCompatibilityToken),
                (override));
    MOCK_METHOD(ResultOrError<Ref<BufferBase>>,
                CreateBufferImpl,
                (const BufferDescriptor*),
                (override));
    MOCK_METHOD(Ref<ComputePipelineBase>,
                CreateUninitializedComputePipelineImpl,
                (const ComputePipelineDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<ExternalTextureBase>>,
                CreateExternalTextureImpl,
                (const ExternalTextureDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<PipelineLayoutBase>>,
                CreatePipelineLayoutImpl,
                (const PipelineLayoutDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<QuerySetBase>>,
                CreateQuerySetImpl,
                (const QuerySetDescriptor*),
                (override));
    MOCK_METHOD(Ref<RenderPipelineBase>,
                CreateUninitializedRenderPipelineImpl,
                (const RenderPipelineDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<SamplerBase>>,
                CreateSamplerImpl,
                (const SamplerDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<ShaderModuleBase>>,
                CreateShaderModuleImpl,
                (const ShaderModuleDescriptor*,
                 ShaderModuleParseResult*,
                 OwnedCompilationMessages*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<SwapChainBase>>,
                CreateSwapChainImpl,
                (const SwapChainDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<NewSwapChainBase>>,
                CreateSwapChainImpl,
                (Surface*, NewSwapChainBase*, const SwapChainDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<TextureBase>>,
                CreateTextureImpl,
                (const TextureDescriptor*),
                (override));
    MOCK_METHOD(ResultOrError<Ref<TextureViewBase>>,
                CreateTextureViewImpl,
                (TextureBase*, const TextureViewDescriptor*),
                (override));

    MOCK_METHOD(MaybeError, TickImpl, (), (override));

    MOCK_METHOD(ResultOrError<ExecutionSerial>, CheckAndUpdateCompletedSerials, (), (override));
    MOCK_METHOD(void, DestroyImpl, (), (override));
    MOCK_METHOD(MaybeError, WaitForIdleForDestruction, (), (override));
};

}  // namespace dawn::native

#endif  // SRC_DAWN_TESTS_UNITTESTS_NATIVE_MOCKS_DEVICEMOCK_H_
