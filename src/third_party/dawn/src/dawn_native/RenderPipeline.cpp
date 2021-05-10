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

#include "dawn_native/RenderPipeline.h"

#include "common/BitSetIterator.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <cmath>

namespace dawn_native {
    // Helper functions
    namespace {

        MaybeError ValidateVertexAttributeDescriptor(
            const VertexAttributeDescriptor* attribute,
            uint64_t vertexBufferStride,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            DAWN_TRY(ValidateVertexFormat(attribute->format));

            if (attribute->shaderLocation >= kMaxVertexAttributes) {
                return DAWN_VALIDATION_ERROR("Setting attribute out of bounds");
            }

            // No underflow is possible because the max vertex format size is smaller than
            // kMaxVertexBufferStride.
            ASSERT(kMaxVertexBufferStride >= VertexFormatSize(attribute->format));
            if (attribute->offset > kMaxVertexBufferStride - VertexFormatSize(attribute->format)) {
                return DAWN_VALIDATION_ERROR("Setting attribute offset out of bounds");
            }

            // No overflow is possible because the offset is already validated to be less
            // than kMaxVertexBufferStride.
            ASSERT(attribute->offset < kMaxVertexBufferStride);
            if (vertexBufferStride > 0 &&
                attribute->offset + VertexFormatSize(attribute->format) > vertexBufferStride) {
                return DAWN_VALIDATION_ERROR("Setting attribute offset out of bounds");
            }

            if (attribute->offset % 4 != 0) {
                return DAWN_VALIDATION_ERROR("Attribute offset needs to be a multiple of 4 bytes");
            }

            if ((*attributesSetMask)[attribute->shaderLocation]) {
                return DAWN_VALIDATION_ERROR("Setting already set attribute");
            }

            attributesSetMask->set(attribute->shaderLocation);
            return {};
        }

        MaybeError ValidateVertexBufferLayoutDescriptor(
            const VertexBufferLayoutDescriptor* buffer,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            DAWN_TRY(ValidateInputStepMode(buffer->stepMode));
            if (buffer->arrayStride > kMaxVertexBufferStride) {
                return DAWN_VALIDATION_ERROR("Setting arrayStride out of bounds");
            }

            if (buffer->arrayStride % 4 != 0) {
                return DAWN_VALIDATION_ERROR(
                    "arrayStride of Vertex buffer needs to be a multiple of 4 bytes");
            }

            for (uint32_t i = 0; i < buffer->attributeCount; ++i) {
                DAWN_TRY(ValidateVertexAttributeDescriptor(&buffer->attributes[i],
                                                           buffer->arrayStride, attributesSetMask));
            }

            return {};
        }

        MaybeError ValidateVertexStateDescriptor(
            DeviceBase* device,
            const VertexStateDescriptor* descriptor,
            wgpu::PrimitiveTopology primitiveTopology,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateIndexFormat(descriptor->indexFormat));

            // Pipeline descriptors must have indexFormat != undefined IFF they are using strip
            // topologies.
            if (IsStripPrimitiveTopology(primitiveTopology)) {
                if (descriptor->indexFormat == wgpu::IndexFormat::Undefined) {
                    return DAWN_VALIDATION_ERROR(
                        "indexFormat must not be undefined when using strip primitive topologies");
                }
            } else if (descriptor->indexFormat != wgpu::IndexFormat::Undefined) {
                return DAWN_VALIDATION_ERROR(
                    "indexFormat must be undefined when using non-strip primitive topologies");
            }

            if (descriptor->vertexBufferCount > kMaxVertexBuffers) {
                return DAWN_VALIDATION_ERROR("Vertex buffer count exceeds maximum");
            }

            uint32_t totalAttributesNum = 0;
            for (uint32_t i = 0; i < descriptor->vertexBufferCount; ++i) {
                DAWN_TRY(ValidateVertexBufferLayoutDescriptor(&descriptor->vertexBuffers[i],
                                                              attributesSetMask));
                totalAttributesNum += descriptor->vertexBuffers[i].attributeCount;
            }

            // Every vertex attribute has a member called shaderLocation, and there are some
            // requirements for shaderLocation: 1) >=0, 2) values are different across different
            // attributes, 3) can't exceed kMaxVertexAttributes. So it can ensure that total
            // attribute number never exceed kMaxVertexAttributes.
            ASSERT(totalAttributesNum <= kMaxVertexAttributes);

            return {};
        }

        MaybeError ValidateRasterizationStateDescriptor(
            const RasterizationStateDescriptor* descriptor) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }

            DAWN_TRY(ValidateFrontFace(descriptor->frontFace));
            DAWN_TRY(ValidateCullMode(descriptor->cullMode));

            if (std::isnan(descriptor->depthBiasSlopeScale) ||
                std::isnan(descriptor->depthBiasClamp)) {
                return DAWN_VALIDATION_ERROR("Depth bias parameters must not be NaN.");
            }

            return {};
        }

        MaybeError ValidateColorStateDescriptor(const DeviceBase* device,
                                                const ColorStateDescriptor& descriptor,
                                                bool fragmentWritten,
                                                wgpu::TextureComponentType fragmentOutputBaseType) {
            if (descriptor.nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateBlendOperation(descriptor.alphaBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor.alphaBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor.alphaBlend.dstFactor));
            DAWN_TRY(ValidateBlendOperation(descriptor.colorBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor.colorBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor.colorBlend.dstFactor));
            DAWN_TRY(ValidateColorWriteMask(descriptor.writeMask));

            const Format* format;
            DAWN_TRY_ASSIGN(format, device->GetInternalFormat(descriptor.format));
            if (!format->IsColor() || !format->isRenderable) {
                return DAWN_VALIDATION_ERROR("Color format must be color renderable");
            }
            if (fragmentWritten &&
                fragmentOutputBaseType != format->GetAspectInfo(Aspect::Color).baseType) {
                return DAWN_VALIDATION_ERROR(
                    "Color format must match the fragment stage output type");
            }

            return {};
        }

        MaybeError ValidateDepthStencilStateDescriptor(
            const DeviceBase* device,
            const DepthStencilStateDescriptor* descriptor) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateCompareFunction(descriptor->depthCompare));
            DAWN_TRY(ValidateCompareFunction(descriptor->stencilFront.compare));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.failOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.depthFailOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.passOp));
            DAWN_TRY(ValidateCompareFunction(descriptor->stencilBack.compare));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.failOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.depthFailOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.passOp));

            const Format* format;
            DAWN_TRY_ASSIGN(format, device->GetInternalFormat(descriptor->format));
            if (!format->HasDepthOrStencil() || !format->isRenderable) {
                return DAWN_VALIDATION_ERROR(
                    "Depth stencil format must be depth-stencil renderable");
            }

            return {};
        }

    }  // anonymous namespace

    // Helper functions
    size_t IndexFormatSize(wgpu::IndexFormat format) {
        switch (format) {
            case wgpu::IndexFormat::Uint16:
                return sizeof(uint16_t);
            case wgpu::IndexFormat::Uint32:
                return sizeof(uint32_t);
            case wgpu::IndexFormat::Undefined:
                UNREACHABLE();
        }
    }

    uint32_t VertexFormatNumComponents(wgpu::VertexFormat format) {
        switch (format) {
            case wgpu::VertexFormat::UChar4:
            case wgpu::VertexFormat::Char4:
            case wgpu::VertexFormat::UChar4Norm:
            case wgpu::VertexFormat::Char4Norm:
            case wgpu::VertexFormat::UShort4:
            case wgpu::VertexFormat::Short4:
            case wgpu::VertexFormat::UShort4Norm:
            case wgpu::VertexFormat::Short4Norm:
            case wgpu::VertexFormat::Half4:
            case wgpu::VertexFormat::Float4:
            case wgpu::VertexFormat::UInt4:
            case wgpu::VertexFormat::Int4:
                return 4;
            case wgpu::VertexFormat::Float3:
            case wgpu::VertexFormat::UInt3:
            case wgpu::VertexFormat::Int3:
                return 3;
            case wgpu::VertexFormat::UChar2:
            case wgpu::VertexFormat::Char2:
            case wgpu::VertexFormat::UChar2Norm:
            case wgpu::VertexFormat::Char2Norm:
            case wgpu::VertexFormat::UShort2:
            case wgpu::VertexFormat::Short2:
            case wgpu::VertexFormat::UShort2Norm:
            case wgpu::VertexFormat::Short2Norm:
            case wgpu::VertexFormat::Half2:
            case wgpu::VertexFormat::Float2:
            case wgpu::VertexFormat::UInt2:
            case wgpu::VertexFormat::Int2:
                return 2;
            case wgpu::VertexFormat::Float:
            case wgpu::VertexFormat::UInt:
            case wgpu::VertexFormat::Int:
                return 1;
        }
    }

    size_t VertexFormatComponentSize(wgpu::VertexFormat format) {
        switch (format) {
            case wgpu::VertexFormat::UChar2:
            case wgpu::VertexFormat::UChar4:
            case wgpu::VertexFormat::Char2:
            case wgpu::VertexFormat::Char4:
            case wgpu::VertexFormat::UChar2Norm:
            case wgpu::VertexFormat::UChar4Norm:
            case wgpu::VertexFormat::Char2Norm:
            case wgpu::VertexFormat::Char4Norm:
                return sizeof(char);
            case wgpu::VertexFormat::UShort2:
            case wgpu::VertexFormat::UShort4:
            case wgpu::VertexFormat::UShort2Norm:
            case wgpu::VertexFormat::UShort4Norm:
            case wgpu::VertexFormat::Short2:
            case wgpu::VertexFormat::Short4:
            case wgpu::VertexFormat::Short2Norm:
            case wgpu::VertexFormat::Short4Norm:
            case wgpu::VertexFormat::Half2:
            case wgpu::VertexFormat::Half4:
                return sizeof(uint16_t);
            case wgpu::VertexFormat::Float:
            case wgpu::VertexFormat::Float2:
            case wgpu::VertexFormat::Float3:
            case wgpu::VertexFormat::Float4:
                return sizeof(float);
            case wgpu::VertexFormat::UInt:
            case wgpu::VertexFormat::UInt2:
            case wgpu::VertexFormat::UInt3:
            case wgpu::VertexFormat::UInt4:
            case wgpu::VertexFormat::Int:
            case wgpu::VertexFormat::Int2:
            case wgpu::VertexFormat::Int3:
            case wgpu::VertexFormat::Int4:
                return sizeof(int32_t);
        }
    }

    size_t VertexFormatSize(wgpu::VertexFormat format) {
        return VertexFormatNumComponents(format) * VertexFormatComponentSize(format);
    }

    bool IsStripPrimitiveTopology(wgpu::PrimitiveTopology primitiveTopology) {
        return primitiveTopology == wgpu::PrimitiveTopology::LineStrip ||
               primitiveTopology == wgpu::PrimitiveTopology::TriangleStrip;
    }

    MaybeError ValidateRenderPipelineDescriptor(DeviceBase* device,
                                                const RenderPipelineDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->layout != nullptr) {
            DAWN_TRY(device->ValidateObject(descriptor->layout));
        }

        // TODO(crbug.com/dawn/136): Support vertex-only pipelines.
        if (descriptor->fragmentStage == nullptr) {
            return DAWN_VALIDATION_ERROR("Null fragment stage is not supported (yet)");
        }

        DAWN_TRY(ValidatePrimitiveTopology(descriptor->primitiveTopology));

        std::bitset<kMaxVertexAttributes> attributesSetMask;
        if (descriptor->vertexState) {
            DAWN_TRY(ValidateVertexStateDescriptor(device,
                descriptor->vertexState, descriptor->primitiveTopology, &attributesSetMask));
        }

        DAWN_TRY(ValidateProgrammableStageDescriptor(
            device, &descriptor->vertexStage, descriptor->layout, SingleShaderStage::Vertex));
        DAWN_TRY(ValidateProgrammableStageDescriptor(
            device, descriptor->fragmentStage, descriptor->layout, SingleShaderStage::Fragment));

        if (descriptor->rasterizationState) {
            DAWN_TRY(ValidateRasterizationStateDescriptor(descriptor->rasterizationState));
        }

        const EntryPointMetadata& vertexMetadata =
            descriptor->vertexStage.module->GetEntryPoint(descriptor->vertexStage.entryPoint);
        if (!IsSubset(vertexMetadata.usedVertexAttributes, attributesSetMask)) {
            return DAWN_VALIDATION_ERROR(
                "Pipeline vertex stage uses vertex buffers not in the vertex state");
        }

        if (!IsValidSampleCount(descriptor->sampleCount)) {
            return DAWN_VALIDATION_ERROR("Sample count is not supported");
        }

        if (descriptor->colorStateCount > kMaxColorAttachments) {
            return DAWN_VALIDATION_ERROR("Color States number exceeds maximum");
        }

        if (descriptor->colorStateCount == 0 && !descriptor->depthStencilState) {
            return DAWN_VALIDATION_ERROR(
                "Should have at least one colorState or a depthStencilState");
        }

        ASSERT(descriptor->fragmentStage != nullptr);
        const EntryPointMetadata& fragmentMetadata =
            descriptor->fragmentStage->module->GetEntryPoint(descriptor->fragmentStage->entryPoint);
        for (ColorAttachmentIndex i(uint8_t(0));
             i < ColorAttachmentIndex(static_cast<uint8_t>(descriptor->colorStateCount)); ++i) {
            DAWN_TRY(ValidateColorStateDescriptor(
                device, descriptor->colorStates[static_cast<uint8_t>(i)],
                fragmentMetadata.fragmentOutputsWritten[i],
                fragmentMetadata.fragmentOutputFormatBaseTypes[i]));
        }

        if (descriptor->depthStencilState) {
            DAWN_TRY(ValidateDepthStencilStateDescriptor(device, descriptor->depthStencilState));
        }

        if (descriptor->alphaToCoverageEnabled && descriptor->sampleCount <= 1) {
            return DAWN_VALIDATION_ERROR("Enabling alphaToCoverage requires sampleCount > 1");
        }

        return {};
    }

    bool StencilTestEnabled(const DepthStencilStateDescriptor* mDepthStencilState) {
        return mDepthStencilState->stencilBack.compare != wgpu::CompareFunction::Always ||
               mDepthStencilState->stencilBack.failOp != wgpu::StencilOperation::Keep ||
               mDepthStencilState->stencilBack.depthFailOp != wgpu::StencilOperation::Keep ||
               mDepthStencilState->stencilBack.passOp != wgpu::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.compare != wgpu::CompareFunction::Always ||
               mDepthStencilState->stencilFront.failOp != wgpu::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.depthFailOp != wgpu::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.passOp != wgpu::StencilOperation::Keep;
    }

    bool BlendEnabled(const ColorStateDescriptor* mColorState) {
        return mColorState->alphaBlend.operation != wgpu::BlendOperation::Add ||
               mColorState->alphaBlend.srcFactor != wgpu::BlendFactor::One ||
               mColorState->alphaBlend.dstFactor != wgpu::BlendFactor::Zero ||
               mColorState->colorBlend.operation != wgpu::BlendOperation::Add ||
               mColorState->colorBlend.srcFactor != wgpu::BlendFactor::One ||
               mColorState->colorBlend.dstFactor != wgpu::BlendFactor::Zero;
    }

    // RenderPipelineBase

    RenderPipelineBase::RenderPipelineBase(DeviceBase* device,
                                           const RenderPipelineDescriptor* descriptor)
        : PipelineBase(device,
                       descriptor->layout,
                       {{SingleShaderStage::Vertex, &descriptor->vertexStage},
                        {SingleShaderStage::Fragment, descriptor->fragmentStage}}),
          mAttachmentState(device->GetOrCreateAttachmentState(descriptor)),
          mPrimitiveTopology(descriptor->primitiveTopology),
          mSampleMask(descriptor->sampleMask),
          mAlphaToCoverageEnabled(descriptor->alphaToCoverageEnabled) {
        if (descriptor->vertexState != nullptr) {
            mVertexState = *descriptor->vertexState;
        } else {
            mVertexState = VertexStateDescriptor();
        }

        for (uint8_t slot = 0; slot < mVertexState.vertexBufferCount; ++slot) {
            if (mVertexState.vertexBuffers[slot].attributeCount == 0) {
                continue;
            }

            VertexBufferSlot typedSlot(slot);

            mVertexBufferSlotsUsed.set(typedSlot);
            mVertexBufferInfos[typedSlot].arrayStride =
                mVertexState.vertexBuffers[slot].arrayStride;
            mVertexBufferInfos[typedSlot].stepMode = mVertexState.vertexBuffers[slot].stepMode;

            for (uint32_t i = 0; i < mVertexState.vertexBuffers[slot].attributeCount; ++i) {
                VertexAttributeLocation location = VertexAttributeLocation(static_cast<uint8_t>(
                    mVertexState.vertexBuffers[slot].attributes[i].shaderLocation));
                mAttributeLocationsUsed.set(location);
                mAttributeInfos[location].shaderLocation = location;
                mAttributeInfos[location].vertexBufferSlot = typedSlot;
                mAttributeInfos[location].offset =
                    mVertexState.vertexBuffers[slot].attributes[i].offset;
                mAttributeInfos[location].format =
                    mVertexState.vertexBuffers[slot].attributes[i].format;
            }
        }

        if (descriptor->rasterizationState != nullptr) {
            mRasterizationState = *descriptor->rasterizationState;
        } else {
            mRasterizationState = RasterizationStateDescriptor();
        }

        if (mAttachmentState->HasDepthStencilAttachment()) {
            mDepthStencilState = *descriptor->depthStencilState;
        } else {
            // These default values below are useful for backends to fill information.
            // The values indicate that depth and stencil test are disabled when backends
            // set their own depth stencil states/descriptors according to the values in
            // mDepthStencilState.
            mDepthStencilState.depthCompare = wgpu::CompareFunction::Always;
            mDepthStencilState.depthWriteEnabled = false;
            mDepthStencilState.stencilBack.compare = wgpu::CompareFunction::Always;
            mDepthStencilState.stencilBack.failOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilBack.depthFailOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilBack.passOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilFront.compare = wgpu::CompareFunction::Always;
            mDepthStencilState.stencilFront.failOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilFront.depthFailOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilFront.passOp = wgpu::StencilOperation::Keep;
            mDepthStencilState.stencilReadMask = 0xff;
            mDepthStencilState.stencilWriteMask = 0xff;
        }

        for (ColorAttachmentIndex i : IterateBitSet(mAttachmentState->GetColorAttachmentsMask())) {
            mColorStates[i] = descriptor->colorStates[static_cast<uint8_t>(i)];
        }

        // TODO(cwallez@chromium.org): Check against the shader module that the correct color
        // attachment are set?
    }

    RenderPipelineBase::RenderPipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : PipelineBase(device, tag) {
    }

    // static
    RenderPipelineBase* RenderPipelineBase::MakeError(DeviceBase* device) {
        return new RenderPipelineBase(device, ObjectBase::kError);
    }

    RenderPipelineBase::~RenderPipelineBase() {
        if (IsCachedReference()) {
            GetDevice()->UncacheRenderPipeline(this);
        }
    }

    const VertexStateDescriptor* RenderPipelineBase::GetVertexStateDescriptor() const {
        ASSERT(!IsError());
        return &mVertexState;
    }

    const ityp::bitset<VertexAttributeLocation, kMaxVertexAttributes>&
    RenderPipelineBase::GetAttributeLocationsUsed() const {
        ASSERT(!IsError());
        return mAttributeLocationsUsed;
    }

    const VertexAttributeInfo& RenderPipelineBase::GetAttribute(
        VertexAttributeLocation location) const {
        ASSERT(!IsError());
        ASSERT(mAttributeLocationsUsed[location]);
        return mAttributeInfos[location];
    }

    const ityp::bitset<VertexBufferSlot, kMaxVertexBuffers>&
    RenderPipelineBase::GetVertexBufferSlotsUsed() const {
        ASSERT(!IsError());
        return mVertexBufferSlotsUsed;
    }

    const VertexBufferInfo& RenderPipelineBase::GetVertexBuffer(VertexBufferSlot slot) const {
        ASSERT(!IsError());
        ASSERT(mVertexBufferSlotsUsed[slot]);
        return mVertexBufferInfos[slot];
    }

    const ColorStateDescriptor* RenderPipelineBase::GetColorStateDescriptor(
        ColorAttachmentIndex attachmentSlot) const {
        ASSERT(!IsError());
        ASSERT(attachmentSlot < mColorStates.size());
        return &mColorStates[attachmentSlot];
    }

    const DepthStencilStateDescriptor* RenderPipelineBase::GetDepthStencilStateDescriptor() const {
        ASSERT(!IsError());
        return &mDepthStencilState;
    }

    wgpu::PrimitiveTopology RenderPipelineBase::GetPrimitiveTopology() const {
        ASSERT(!IsError());
        return mPrimitiveTopology;
    }

    wgpu::CullMode RenderPipelineBase::GetCullMode() const {
        ASSERT(!IsError());
        return mRasterizationState.cullMode;
    }

    wgpu::FrontFace RenderPipelineBase::GetFrontFace() const {
        ASSERT(!IsError());
        return mRasterizationState.frontFace;
    }

    bool RenderPipelineBase::IsDepthBiasEnabled() const {
        ASSERT(!IsError());
        return mRasterizationState.depthBias != 0 || mRasterizationState.depthBiasSlopeScale != 0;
    }

    int32_t RenderPipelineBase::GetDepthBias() const {
        ASSERT(!IsError());
        return mRasterizationState.depthBias;
    }

    float RenderPipelineBase::GetDepthBiasSlopeScale() const {
        ASSERT(!IsError());
        return mRasterizationState.depthBiasSlopeScale;
    }

    float RenderPipelineBase::GetDepthBiasClamp() const {
        ASSERT(!IsError());
        return mRasterizationState.depthBiasClamp;
    }

    ityp::bitset<ColorAttachmentIndex, kMaxColorAttachments>
    RenderPipelineBase::GetColorAttachmentsMask() const {
        ASSERT(!IsError());
        return mAttachmentState->GetColorAttachmentsMask();
    }

    bool RenderPipelineBase::HasDepthStencilAttachment() const {
        ASSERT(!IsError());
        return mAttachmentState->HasDepthStencilAttachment();
    }

    wgpu::TextureFormat RenderPipelineBase::GetColorAttachmentFormat(
        ColorAttachmentIndex attachment) const {
        ASSERT(!IsError());
        return mColorStates[attachment].format;
    }

    wgpu::TextureFormat RenderPipelineBase::GetDepthStencilFormat() const {
        ASSERT(!IsError());
        ASSERT(mAttachmentState->HasDepthStencilAttachment());
        return mDepthStencilState.format;
    }

    uint32_t RenderPipelineBase::GetSampleCount() const {
        ASSERT(!IsError());
        return mAttachmentState->GetSampleCount();
    }

    uint32_t RenderPipelineBase::GetSampleMask() const {
        ASSERT(!IsError());
        return mSampleMask;
    }

    bool RenderPipelineBase::IsAlphaToCoverageEnabled() const {
        ASSERT(!IsError());
        return mAlphaToCoverageEnabled;
    }

    const AttachmentState* RenderPipelineBase::GetAttachmentState() const {
        ASSERT(!IsError());

        return mAttachmentState.Get();
    }

    size_t RenderPipelineBase::ComputeContentHash() {
        ObjectContentHasher recorder;

        // Record modules and layout
        recorder.Record(PipelineBase::ComputeContentHash());

        // Hierarchically record the attachment state.
        // It contains the attachments set, texture formats, and sample count.
        recorder.Record(mAttachmentState->GetContentHash());

        // Record attachments
        for (ColorAttachmentIndex i : IterateBitSet(mAttachmentState->GetColorAttachmentsMask())) {
            const ColorStateDescriptor& desc = *GetColorStateDescriptor(i);
            recorder.Record(desc.writeMask);
            recorder.Record(desc.colorBlend.operation, desc.colorBlend.srcFactor,
                            desc.colorBlend.dstFactor);
            recorder.Record(desc.alphaBlend.operation, desc.alphaBlend.srcFactor,
                            desc.alphaBlend.dstFactor);
        }

        if (mAttachmentState->HasDepthStencilAttachment()) {
            const DepthStencilStateDescriptor& desc = mDepthStencilState;
            recorder.Record(desc.depthWriteEnabled, desc.depthCompare);
            recorder.Record(desc.stencilReadMask, desc.stencilWriteMask);
            recorder.Record(desc.stencilFront.compare, desc.stencilFront.failOp,
                            desc.stencilFront.depthFailOp, desc.stencilFront.passOp);
            recorder.Record(desc.stencilBack.compare, desc.stencilBack.failOp,
                            desc.stencilBack.depthFailOp, desc.stencilBack.passOp);
        }

        // Record vertex state
        recorder.Record(mAttributeLocationsUsed);
        for (VertexAttributeLocation location : IterateBitSet(mAttributeLocationsUsed)) {
            const VertexAttributeInfo& desc = GetAttribute(location);
            recorder.Record(desc.shaderLocation, desc.vertexBufferSlot, desc.offset, desc.format);
        }

        recorder.Record(mVertexBufferSlotsUsed);
        for (VertexBufferSlot slot : IterateBitSet(mVertexBufferSlotsUsed)) {
            const VertexBufferInfo& desc = GetVertexBuffer(slot);
            recorder.Record(desc.arrayStride, desc.stepMode);
        }

        recorder.Record(mVertexState.indexFormat);

        // Record rasterization state
        {
            const RasterizationStateDescriptor& desc = mRasterizationState;
            recorder.Record(desc.frontFace, desc.cullMode);
            recorder.Record(desc.depthBias, desc.depthBiasSlopeScale, desc.depthBiasClamp);
        }

        // Record other state
        recorder.Record(mPrimitiveTopology, mSampleMask, mAlphaToCoverageEnabled);

        return recorder.GetContentHash();
    }

    bool RenderPipelineBase::EqualityFunc::operator()(const RenderPipelineBase* a,
                                                      const RenderPipelineBase* b) const {
        // Check the layout and shader stages.
        if (!PipelineBase::EqualForCache(a, b)) {
            return false;
        }

        // Check the attachment state.
        // It contains the attachments set, texture formats, and sample count.
        if (a->mAttachmentState.Get() != b->mAttachmentState.Get()) {
            return false;
        }

        for (ColorAttachmentIndex i :
             IterateBitSet(a->mAttachmentState->GetColorAttachmentsMask())) {
            const ColorStateDescriptor& descA = *a->GetColorStateDescriptor(i);
            const ColorStateDescriptor& descB = *b->GetColorStateDescriptor(i);
            if (descA.writeMask != descB.writeMask) {
                return false;
            }
            if (descA.colorBlend.operation != descB.colorBlend.operation ||
                descA.colorBlend.srcFactor != descB.colorBlend.srcFactor ||
                descA.colorBlend.dstFactor != descB.colorBlend.dstFactor) {
                return false;
            }
            if (descA.alphaBlend.operation != descB.alphaBlend.operation ||
                descA.alphaBlend.srcFactor != descB.alphaBlend.srcFactor ||
                descA.alphaBlend.dstFactor != descB.alphaBlend.dstFactor) {
                return false;
            }
        }

        if (a->mAttachmentState->HasDepthStencilAttachment()) {
            const DepthStencilStateDescriptor& descA = a->mDepthStencilState;
            const DepthStencilStateDescriptor& descB = b->mDepthStencilState;
            if (descA.depthWriteEnabled != descB.depthWriteEnabled ||
                descA.depthCompare != descB.depthCompare) {
                return false;
            }
            if (descA.stencilReadMask != descB.stencilReadMask ||
                descA.stencilWriteMask != descB.stencilWriteMask) {
                return false;
            }
            if (descA.stencilFront.compare != descB.stencilFront.compare ||
                descA.stencilFront.failOp != descB.stencilFront.failOp ||
                descA.stencilFront.depthFailOp != descB.stencilFront.depthFailOp ||
                descA.stencilFront.passOp != descB.stencilFront.passOp) {
                return false;
            }
            if (descA.stencilBack.compare != descB.stencilBack.compare ||
                descA.stencilBack.failOp != descB.stencilBack.failOp ||
                descA.stencilBack.depthFailOp != descB.stencilBack.depthFailOp ||
                descA.stencilBack.passOp != descB.stencilBack.passOp) {
                return false;
            }
        }

        // Check vertex state
        if (a->mAttributeLocationsUsed != b->mAttributeLocationsUsed) {
            return false;
        }

        for (VertexAttributeLocation loc : IterateBitSet(a->mAttributeLocationsUsed)) {
            const VertexAttributeInfo& descA = a->GetAttribute(loc);
            const VertexAttributeInfo& descB = b->GetAttribute(loc);
            if (descA.shaderLocation != descB.shaderLocation ||
                descA.vertexBufferSlot != descB.vertexBufferSlot || descA.offset != descB.offset ||
                descA.format != descB.format) {
                return false;
            }
        }

        if (a->mVertexBufferSlotsUsed != b->mVertexBufferSlotsUsed) {
            return false;
        }

        for (VertexBufferSlot slot : IterateBitSet(a->mVertexBufferSlotsUsed)) {
            const VertexBufferInfo& descA = a->GetVertexBuffer(slot);
            const VertexBufferInfo& descB = b->GetVertexBuffer(slot);
            if (descA.arrayStride != descB.arrayStride || descA.stepMode != descB.stepMode) {
                return false;
            }
        }

        if (a->mVertexState.indexFormat != b->mVertexState.indexFormat) {
            return false;
        }

        // Check rasterization state
        {
            const RasterizationStateDescriptor& descA = a->mRasterizationState;
            const RasterizationStateDescriptor& descB = b->mRasterizationState;
            if (descA.frontFace != descB.frontFace || descA.cullMode != descB.cullMode) {
                return false;
            }

            ASSERT(!std::isnan(descA.depthBiasSlopeScale));
            ASSERT(!std::isnan(descB.depthBiasSlopeScale));
            ASSERT(!std::isnan(descA.depthBiasClamp));
            ASSERT(!std::isnan(descB.depthBiasClamp));

            if (descA.depthBias != descB.depthBias ||
                descA.depthBiasSlopeScale != descB.depthBiasSlopeScale ||
                descA.depthBiasClamp != descB.depthBiasClamp) {
                return false;
            }
        }

        // Check other state
        if (a->mPrimitiveTopology != b->mPrimitiveTopology || a->mSampleMask != b->mSampleMask ||
            a->mAlphaToCoverageEnabled != b->mAlphaToCoverageEnabled) {
            return false;
        }

        return true;
    }

}  // namespace dawn_native
