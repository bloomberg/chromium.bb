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
#include "dawn_native/Texture.h"
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {
    // Helper functions
    namespace {

        MaybeError ValidateVertexInputDescriptor(const VertexInputDescriptor* input,
                                                 std::bitset<kMaxVertexInputs>* inputsSetMask) {
            DAWN_TRY(ValidateInputStepMode(input->stepMode));
            if (input->inputSlot >= kMaxVertexInputs) {
                return DAWN_VALIDATION_ERROR("Setting input out of bounds");
            }
            if (input->stride > kMaxVertexInputStride) {
                return DAWN_VALIDATION_ERROR("Setting input stride out of bounds");
            }
            if ((*inputsSetMask)[input->inputSlot]) {
                return DAWN_VALIDATION_ERROR("Setting already set input");
            }

            inputsSetMask->set(input->inputSlot);
            return {};
        }

        MaybeError ValidateVertexAttributeDescriptor(
            const VertexAttributeDescriptor* attribute,
            const std::bitset<kMaxVertexInputs>* inputsSetMask,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            DAWN_TRY(ValidateVertexFormat(attribute->format));

            if (attribute->shaderLocation >= kMaxVertexAttributes) {
                return DAWN_VALIDATION_ERROR("Setting attribute out of bounds");
            }
            if (attribute->inputSlot >= kMaxVertexInputs) {
                return DAWN_VALIDATION_ERROR("Binding slot out of bounds");
            }
            ASSERT(kMaxVertexAttributeEnd >= VertexFormatSize(attribute->format));
            if (attribute->offset > kMaxVertexAttributeEnd - VertexFormatSize(attribute->format)) {
                return DAWN_VALIDATION_ERROR("Setting attribute offset out of bounds");
            }
            if ((*attributesSetMask)[attribute->shaderLocation]) {
                return DAWN_VALIDATION_ERROR("Setting already set attribute");
            }
            if (!(*inputsSetMask)[attribute->inputSlot]) {
                return DAWN_VALIDATION_ERROR(
                    "Vertex attribute slot doesn't match any vertex input slot");
            }

            attributesSetMask->set(attribute->shaderLocation);
            return {};
        }

        MaybeError ValidateInputStateDescriptor(
            const InputStateDescriptor* descriptor,
            std::bitset<kMaxVertexInputs>* inputsSetMask,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateIndexFormat(descriptor->indexFormat));

            if (descriptor->numInputs > kMaxVertexInputs) {
                return DAWN_VALIDATION_ERROR("Vertex Inputs number exceeds maximum");
            }
            if (descriptor->numAttributes > kMaxVertexAttributes) {
                return DAWN_VALIDATION_ERROR("Vertex Attributes number exceeds maximum");
            }

            for (uint32_t i = 0; i < descriptor->numInputs; ++i) {
                DAWN_TRY(ValidateVertexInputDescriptor(&descriptor->inputs[i], inputsSetMask));
            }

            for (uint32_t i = 0; i < descriptor->numAttributes; ++i) {
                DAWN_TRY(ValidateVertexAttributeDescriptor(&descriptor->attributes[i],
                                                           inputsSetMask, attributesSetMask));
            }

            return {};
        }

        MaybeError ValidateRasterizationStateDescriptor(
            const RasterizationStateDescriptor* descriptor) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateFrontFace(descriptor->frontFace));
            DAWN_TRY(ValidateCullMode(descriptor->cullMode));
            return {};
        }

        MaybeError ValidateColorStateDescriptor(const ColorStateDescriptor* descriptor) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateBlendOperation(descriptor->alphaBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor->alphaBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor->alphaBlend.dstFactor));
            DAWN_TRY(ValidateBlendOperation(descriptor->colorBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor->colorBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor->colorBlend.dstFactor));
            DAWN_TRY(ValidateColorWriteMask(descriptor->writeMask));

            dawn::TextureFormat format = descriptor->format;
            DAWN_TRY(ValidateTextureFormat(format));
            if (!IsColorRenderableTextureFormat(format)) {
                return DAWN_VALIDATION_ERROR("Color format must be color renderable");
            }

            return {};
        }

        MaybeError ValidateDepthStencilStateDescriptor(
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

            dawn::TextureFormat format = descriptor->format;
            DAWN_TRY(ValidateTextureFormat(format));
            if (!IsDepthStencilRenderableTextureFormat(format)) {
                return DAWN_VALIDATION_ERROR(
                    "Depth stencil format must be depth-stencil renderable");
            }

            return {};
        }

    }  // anonymous namespace

    // Helper functions
    size_t IndexFormatSize(dawn::IndexFormat format) {
        switch (format) {
            case dawn::IndexFormat::Uint16:
                return sizeof(uint16_t);
            case dawn::IndexFormat::Uint32:
                return sizeof(uint32_t);
            default:
                UNREACHABLE();
        }
    }

    uint32_t VertexFormatNumComponents(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::UChar4:
            case dawn::VertexFormat::Char4:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::Char4Norm:
            case dawn::VertexFormat::UShort4:
            case dawn::VertexFormat::Short4:
            case dawn::VertexFormat::UShort4Norm:
            case dawn::VertexFormat::Short4Norm:
            case dawn::VertexFormat::Half4:
            case dawn::VertexFormat::Float4:
            case dawn::VertexFormat::UInt4:
            case dawn::VertexFormat::Int4:
                return 4;
            case dawn::VertexFormat::Float3:
            case dawn::VertexFormat::UInt3:
            case dawn::VertexFormat::Int3:
                return 3;
            case dawn::VertexFormat::UChar2:
            case dawn::VertexFormat::Char2:
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::Char2Norm:
            case dawn::VertexFormat::UShort2:
            case dawn::VertexFormat::Short2:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::Short2Norm:
            case dawn::VertexFormat::Half2:
            case dawn::VertexFormat::Float2:
            case dawn::VertexFormat::UInt2:
            case dawn::VertexFormat::Int2:
                return 2;
            case dawn::VertexFormat::Float:
            case dawn::VertexFormat::UInt:
            case dawn::VertexFormat::Int:
                return 1;
            default:
                UNREACHABLE();
        }
    }

    size_t VertexFormatComponentSize(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::UChar2:
            case dawn::VertexFormat::UChar4:
            case dawn::VertexFormat::Char2:
            case dawn::VertexFormat::Char4:
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::Char2Norm:
            case dawn::VertexFormat::Char4Norm:
                return sizeof(char);
            case dawn::VertexFormat::UShort2:
            case dawn::VertexFormat::UShort4:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::UShort4Norm:
            case dawn::VertexFormat::Short2:
            case dawn::VertexFormat::Short4:
            case dawn::VertexFormat::Short2Norm:
            case dawn::VertexFormat::Short4Norm:
            case dawn::VertexFormat::Half2:
            case dawn::VertexFormat::Half4:
                return sizeof(uint16_t);
            case dawn::VertexFormat::Float:
            case dawn::VertexFormat::Float2:
            case dawn::VertexFormat::Float3:
            case dawn::VertexFormat::Float4:
                return sizeof(float);
            case dawn::VertexFormat::UInt:
            case dawn::VertexFormat::UInt2:
            case dawn::VertexFormat::UInt3:
            case dawn::VertexFormat::UInt4:
            case dawn::VertexFormat::Int:
            case dawn::VertexFormat::Int2:
            case dawn::VertexFormat::Int3:
            case dawn::VertexFormat::Int4:
                return sizeof(int32_t);
            default:
                UNREACHABLE();
        }
    }

    size_t VertexFormatSize(dawn::VertexFormat format) {
        return VertexFormatNumComponents(format) * VertexFormatComponentSize(format);
    }

    MaybeError ValidateRenderPipelineDescriptor(DeviceBase* device,
                                                const RenderPipelineDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        DAWN_TRY(device->ValidateObject(descriptor->layout));

        if (descriptor->inputState == nullptr) {
            return DAWN_VALIDATION_ERROR("Input state must not be null");
        }

        std::bitset<kMaxVertexInputs> inputsSetMask;
        std::bitset<kMaxVertexAttributes> attributesSetMask;
        DAWN_TRY(ValidateInputStateDescriptor(descriptor->inputState, &inputsSetMask,
                                              &attributesSetMask));
        DAWN_TRY(ValidatePrimitiveTopology(descriptor->primitiveTopology));
        DAWN_TRY(ValidatePipelineStageDescriptor(device, descriptor->vertexStage,
                                                 descriptor->layout, dawn::ShaderStage::Vertex));
        DAWN_TRY(ValidatePipelineStageDescriptor(device, descriptor->fragmentStage,
                                                 descriptor->layout, dawn::ShaderStage::Fragment));
        DAWN_TRY(ValidateRasterizationStateDescriptor(descriptor->rasterizationState));

        if ((descriptor->vertexStage->module->GetUsedVertexAttributes() & ~attributesSetMask)
                .any()) {
            return DAWN_VALIDATION_ERROR(
                "Pipeline vertex stage uses inputs not in the input state");
        }

        if (!IsValidSampleCount(descriptor->sampleCount)) {
            return DAWN_VALIDATION_ERROR("Sample count is not supported");
        }

        if (descriptor->colorStateCount > kMaxColorAttachments) {
            return DAWN_VALIDATION_ERROR("Color States number exceeds maximum");
        }

        if (descriptor->colorStateCount == 0 && !descriptor->depthStencilState) {
            return DAWN_VALIDATION_ERROR("Should have at least one attachment");
        }

        for (uint32_t i = 0; i < descriptor->colorStateCount; ++i) {
            DAWN_TRY(ValidateColorStateDescriptor(descriptor->colorStates[i]));
        }

        if (descriptor->depthStencilState) {
            DAWN_TRY(ValidateDepthStencilStateDescriptor(descriptor->depthStencilState));
        }

        return {};
    }

    bool StencilTestEnabled(const DepthStencilStateDescriptor* mDepthStencilState) {
        return mDepthStencilState->stencilBack.compare != dawn::CompareFunction::Always ||
               mDepthStencilState->stencilBack.failOp != dawn::StencilOperation::Keep ||
               mDepthStencilState->stencilBack.depthFailOp != dawn::StencilOperation::Keep ||
               mDepthStencilState->stencilBack.passOp != dawn::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.compare != dawn::CompareFunction::Always ||
               mDepthStencilState->stencilFront.failOp != dawn::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.depthFailOp != dawn::StencilOperation::Keep ||
               mDepthStencilState->stencilFront.passOp != dawn::StencilOperation::Keep;
    }

    bool BlendEnabled(const ColorStateDescriptor* mColorState) {
        return mColorState->alphaBlend.operation != dawn::BlendOperation::Add ||
               mColorState->alphaBlend.srcFactor != dawn::BlendFactor::One ||
               mColorState->alphaBlend.dstFactor != dawn::BlendFactor::Zero ||
               mColorState->colorBlend.operation != dawn::BlendOperation::Add ||
               mColorState->colorBlend.srcFactor != dawn::BlendFactor::One ||
               mColorState->colorBlend.dstFactor != dawn::BlendFactor::Zero;
    }

    // RenderPipelineBase

    RenderPipelineBase::RenderPipelineBase(DeviceBase* device,
                                           const RenderPipelineDescriptor* descriptor)
        : PipelineBase(device,
                       descriptor->layout,
                       dawn::ShaderStageBit::Vertex | dawn::ShaderStageBit::Fragment),
          mInputState(*descriptor->inputState),
          mPrimitiveTopology(descriptor->primitiveTopology),
          mRasterizationState(*descriptor->rasterizationState),
          mHasDepthStencilAttachment(descriptor->depthStencilState != nullptr),
          mSampleCount(descriptor->sampleCount) {
        uint32_t location = 0;
        for (uint32_t i = 0; i < mInputState.numAttributes; ++i) {
            location = mInputState.attributes[i].shaderLocation;
            mAttributesSetMask.set(location);
            mAttributeInfos[location] = mInputState.attributes[i];
        }
        uint32_t slot = 0;
        for (uint32_t i = 0; i < mInputState.numInputs; ++i) {
            slot = mInputState.inputs[i].inputSlot;
            mInputsSetMask.set(slot);
            mInputInfos[slot] = mInputState.inputs[i];
        }

        if (mHasDepthStencilAttachment) {
            mDepthStencilState = *descriptor->depthStencilState;
        } else {
            // These default values below are useful for backends to fill information.
            // The values indicate that depth and stencil test are disabled when backends
            // set their own depth stencil states/descriptors according to the values in
            // mDepthStencilState.
            mDepthStencilState.depthCompare = dawn::CompareFunction::Always;
            mDepthStencilState.depthWriteEnabled = false;
            mDepthStencilState.stencilBack.compare = dawn::CompareFunction::Always;
            mDepthStencilState.stencilBack.failOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilBack.depthFailOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilBack.passOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilFront.compare = dawn::CompareFunction::Always;
            mDepthStencilState.stencilFront.failOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilFront.depthFailOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilFront.passOp = dawn::StencilOperation::Keep;
            mDepthStencilState.stencilReadMask = 0xff;
            mDepthStencilState.stencilWriteMask = 0xff;
        }
        ExtractModuleData(dawn::ShaderStage::Vertex, descriptor->vertexStage->module);
        ExtractModuleData(dawn::ShaderStage::Fragment, descriptor->fragmentStage->module);

        for (uint32_t i = 0; i < descriptor->colorStateCount; ++i) {
            mColorAttachmentsSet.set(i);
            mColorStates[i] = *descriptor->colorStates[i];
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

    const InputStateDescriptor* RenderPipelineBase::GetInputStateDescriptor() const {
        ASSERT(!IsError());
        return &mInputState;
    }

    const std::bitset<kMaxVertexAttributes>& RenderPipelineBase::GetAttributesSetMask() const {
        ASSERT(!IsError());
        return mAttributesSetMask;
    }

    const VertexAttributeDescriptor& RenderPipelineBase::GetAttribute(uint32_t location) const {
        ASSERT(!IsError());
        ASSERT(mAttributesSetMask[location]);
        return mAttributeInfos[location];
    }

    const std::bitset<kMaxVertexInputs>& RenderPipelineBase::GetInputsSetMask() const {
        ASSERT(!IsError());
        return mInputsSetMask;
    }

    const VertexInputDescriptor& RenderPipelineBase::GetInput(uint32_t slot) const {
        ASSERT(!IsError());
        ASSERT(mInputsSetMask[slot]);
        return mInputInfos[slot];
    }

    const ColorStateDescriptor* RenderPipelineBase::GetColorStateDescriptor(
        uint32_t attachmentSlot) {
        ASSERT(!IsError());
        ASSERT(attachmentSlot < mColorStates.size());
        return &mColorStates[attachmentSlot];
    }

    const DepthStencilStateDescriptor* RenderPipelineBase::GetDepthStencilStateDescriptor() {
        ASSERT(!IsError());
        return &mDepthStencilState;
    }

    dawn::PrimitiveTopology RenderPipelineBase::GetPrimitiveTopology() const {
        ASSERT(!IsError());
        return mPrimitiveTopology;
    }

    std::bitset<kMaxColorAttachments> RenderPipelineBase::GetColorAttachmentsMask() const {
        ASSERT(!IsError());
        return mColorAttachmentsSet;
    }

    bool RenderPipelineBase::HasDepthStencilAttachment() const {
        ASSERT(!IsError());
        return mHasDepthStencilAttachment;
    }

    dawn::TextureFormat RenderPipelineBase::GetColorAttachmentFormat(uint32_t attachment) const {
        ASSERT(!IsError());
        return mColorStates[attachment].format;
    }

    dawn::TextureFormat RenderPipelineBase::GetDepthStencilFormat() const {
        ASSERT(!IsError());
        ASSERT(mHasDepthStencilAttachment);
        return mDepthStencilState.format;
    }

    uint32_t RenderPipelineBase::GetSampleCount() const {
        ASSERT(!IsError());
        return mSampleCount;
    }

    bool RenderPipelineBase::IsCompatibleWith(const BeginRenderPassCmd* renderPass) const {
        ASSERT(!IsError());
        // TODO(cwallez@chromium.org): This is called on every SetPipeline command. Optimize it for
        // example by caching some "attachment compatibility" object that would make the
        // compatibility check a single pointer comparison.

        if (renderPass->colorAttachmentsSet != mColorAttachmentsSet) {
            return false;
        }

        for (uint32_t i : IterateBitSet(mColorAttachmentsSet)) {
            if (renderPass->colorAttachments[i].view->GetFormat() != mColorStates[i].format) {
                return false;
            }
        }

        if (renderPass->hasDepthStencilAttachment != mHasDepthStencilAttachment) {
            return false;
        }

        if (mHasDepthStencilAttachment &&
            (renderPass->depthStencilAttachment.view->GetFormat() != mDepthStencilState.format)) {
            return false;
        }

        if (renderPass->sampleCount != mSampleCount) {
            return false;
        }

        return true;
    }

    std::bitset<kMaxVertexAttributes> RenderPipelineBase::GetAttributesUsingInput(
        uint32_t slot) const {
        ASSERT(!IsError());
        return attributesUsingInput[slot];
    }

}  // namespace dawn_native
