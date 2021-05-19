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

#include "dawn_native/RenderBundleEncoder.h"

#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/Format.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native {

    MaybeError ValidateColorAttachmentFormat(const DeviceBase* device,
                                             wgpu::TextureFormat textureFormat) {
        DAWN_TRY(ValidateTextureFormat(textureFormat));
        const Format* format = nullptr;
        DAWN_TRY_ASSIGN(format, device->GetInternalFormat(textureFormat));
        if (!format->IsColor() || !format->isRenderable) {
            return DAWN_VALIDATION_ERROR(
                "The color attachment texture format is not color renderable");
        }
        return {};
    }

    MaybeError ValidateDepthStencilAttachmentFormat(const DeviceBase* device,
                                                    wgpu::TextureFormat textureFormat) {
        DAWN_TRY(ValidateTextureFormat(textureFormat));
        const Format* format = nullptr;
        DAWN_TRY_ASSIGN(format, device->GetInternalFormat(textureFormat));
        if (!format->HasDepthOrStencil() || !format->isRenderable) {
            return DAWN_VALIDATION_ERROR(
                "The depth stencil attachment texture format is not a renderable depth/stencil "
                "format");
        }
        return {};
    }

    MaybeError ValidateRenderBundleEncoderDescriptor(
        const DeviceBase* device,
        const RenderBundleEncoderDescriptor* descriptor) {
        if (!IsValidSampleCount(descriptor->sampleCount)) {
            return DAWN_VALIDATION_ERROR("Sample count is not supported");
        }

        if (descriptor->colorFormatsCount > kMaxColorAttachments) {
            return DAWN_VALIDATION_ERROR("Color formats count exceeds maximum");
        }

        if (descriptor->colorFormatsCount == 0 &&
            descriptor->depthStencilFormat == wgpu::TextureFormat::Undefined) {
            return DAWN_VALIDATION_ERROR("Should have at least one attachment format");
        }

        for (uint32_t i = 0; i < descriptor->colorFormatsCount; ++i) {
            DAWN_TRY(ValidateColorAttachmentFormat(device, descriptor->colorFormats[i]));
        }

        if (descriptor->depthStencilFormat != wgpu::TextureFormat::Undefined) {
            DAWN_TRY(ValidateDepthStencilAttachmentFormat(device, descriptor->depthStencilFormat));
        }

        return {};
    }

    RenderBundleEncoder::RenderBundleEncoder(DeviceBase* device,
                                             const RenderBundleEncoderDescriptor* descriptor)
        : RenderEncoderBase(device,
                            &mBundleEncodingContext,
                            device->GetOrCreateAttachmentState(descriptor)),
          mBundleEncodingContext(device, this) {
    }

    RenderBundleEncoder::RenderBundleEncoder(DeviceBase* device, ErrorTag errorTag)
        : RenderEncoderBase(device, &mBundleEncodingContext, errorTag),
          mBundleEncodingContext(device, this) {
    }

    // static
    Ref<RenderBundleEncoder> RenderBundleEncoder::Create(
        DeviceBase* device,
        const RenderBundleEncoderDescriptor* descriptor) {
        return AcquireRef(new RenderBundleEncoder(device, descriptor));
    }

    // static
    RenderBundleEncoder* RenderBundleEncoder::MakeError(DeviceBase* device) {
        return new RenderBundleEncoder(device, ObjectBase::kError);
    }

    CommandIterator RenderBundleEncoder::AcquireCommands() {
        return mBundleEncodingContext.AcquireCommands();
    }

    RenderBundleBase* RenderBundleEncoder::APIFinish(const RenderBundleDescriptor* descriptor) {
        RenderBundleBase* result = nullptr;

        if (GetDevice()->ConsumedError(FinishImpl(descriptor), &result)) {
            return RenderBundleBase::MakeError(GetDevice());
        }

        return result;
    }

    ResultOrError<RenderBundleBase*> RenderBundleEncoder::FinishImpl(
        const RenderBundleDescriptor* descriptor) {
        // Even if mBundleEncodingContext.Finish() validation fails, calling it will mutate the
        // internal state of the encoding context. Subsequent calls to encode commands will generate
        // errors.
        DAWN_TRY(mBundleEncodingContext.Finish());

        PassResourceUsage usages = mUsageTracker.AcquireResourceUsage();
        if (IsValidationEnabled()) {
            DAWN_TRY(GetDevice()->ValidateObject(this));
            DAWN_TRY(ValidateProgrammableEncoderEnd());
            DAWN_TRY(ValidateFinish(mBundleEncodingContext.GetIterator(), usages));
        }

        return new RenderBundleBase(this, descriptor, AcquireAttachmentState(), std::move(usages));
    }

    MaybeError RenderBundleEncoder::ValidateFinish(CommandIterator* commands,
                                                   const PassResourceUsage& usages) const {
        TRACE_EVENT0(GetDevice()->GetPlatform(), Validation, "RenderBundleEncoder::ValidateFinish");
        DAWN_TRY(GetDevice()->ValidateObject(this));
        DAWN_TRY(ValidatePassResourceUsage(usages));
        return {};
    }

}  // namespace dawn_native
