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

#include "dawn_native/Sampler.h"

#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <cmath>

namespace dawn_native {

    MaybeError ValidateSamplerDescriptor(DeviceBase*, const SamplerDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (std::isnan(descriptor->lodMinClamp) || std::isnan(descriptor->lodMaxClamp)) {
            return DAWN_VALIDATION_ERROR("LOD clamp bounds must not be NaN");
        }

        if (descriptor->lodMinClamp < 0 || descriptor->lodMaxClamp < 0) {
            return DAWN_VALIDATION_ERROR("LOD clamp bounds must be positive");
        }

        if (descriptor->lodMinClamp > descriptor->lodMaxClamp) {
            return DAWN_VALIDATION_ERROR(
                "Min lod clamp value cannot greater than max lod clamp value");
        }

        if (descriptor->maxAnisotropy > 1) {
            if (descriptor->minFilter != wgpu::FilterMode::Linear ||
                descriptor->magFilter != wgpu::FilterMode::Linear ||
                descriptor->mipmapFilter != wgpu::FilterMode::Linear) {
                return DAWN_VALIDATION_ERROR(
                    "min, mag, and mipmap filter should be linear when using anisotropic "
                    "filtering");
            }
        } else if (descriptor->maxAnisotropy == 0u) {
            return DAWN_VALIDATION_ERROR("max anisotropy cannot be set to 0");
        }

        DAWN_TRY(ValidateFilterMode(descriptor->minFilter));
        DAWN_TRY(ValidateFilterMode(descriptor->magFilter));
        DAWN_TRY(ValidateFilterMode(descriptor->mipmapFilter));
        DAWN_TRY(ValidateAddressMode(descriptor->addressModeU));
        DAWN_TRY(ValidateAddressMode(descriptor->addressModeV));
        DAWN_TRY(ValidateAddressMode(descriptor->addressModeW));

        // CompareFunction::Undefined is tagged as invalid because it can't be used, except for the
        // SamplerDescriptor where it is a special value that means the sampler is not a
        // comparison-sampler.
        if (descriptor->compare != wgpu::CompareFunction::Undefined) {
            DAWN_TRY(ValidateCompareFunction(descriptor->compare));
        }

        return {};
    }

    // SamplerBase

    SamplerBase::SamplerBase(DeviceBase* device, const SamplerDescriptor* descriptor)
        : CachedObject(device),
          mAddressModeU(descriptor->addressModeU),
          mAddressModeV(descriptor->addressModeV),
          mAddressModeW(descriptor->addressModeW),
          mMagFilter(descriptor->magFilter),
          mMinFilter(descriptor->minFilter),
          mMipmapFilter(descriptor->mipmapFilter),
          mLodMinClamp(descriptor->lodMinClamp),
          mLodMaxClamp(descriptor->lodMaxClamp),
          mCompareFunction(descriptor->compare),
          mMaxAnisotropy(descriptor->maxAnisotropy) {
    }

    SamplerBase::SamplerBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : CachedObject(device, tag) {
    }

    SamplerBase::~SamplerBase() {
        if (IsCachedReference()) {
            GetDevice()->UncacheSampler(this);
        }
    }

    // static
    SamplerBase* SamplerBase::MakeError(DeviceBase* device) {
        return new SamplerBase(device, ObjectBase::kError);
    }

    bool SamplerBase::HasCompareFunction() const {
        return mCompareFunction != wgpu::CompareFunction::Undefined;
    }

    size_t SamplerBase::ComputeContentHash() {
        ObjectContentHasher recorder;
        recorder.Record(mAddressModeU, mAddressModeV, mAddressModeW, mMagFilter, mMinFilter,
                        mMipmapFilter, mLodMinClamp, mLodMaxClamp, mCompareFunction,
                        mMaxAnisotropy);
        return recorder.GetContentHash();
    }

    bool SamplerBase::EqualityFunc::operator()(const SamplerBase* a, const SamplerBase* b) const {
        if (a == b) {
            return true;
        }

        ASSERT(!std::isnan(a->mLodMinClamp));
        ASSERT(!std::isnan(b->mLodMinClamp));
        ASSERT(!std::isnan(a->mLodMaxClamp));
        ASSERT(!std::isnan(b->mLodMaxClamp));

        return a->mAddressModeU == b->mAddressModeU && a->mAddressModeV == b->mAddressModeV &&
               a->mAddressModeW == b->mAddressModeW && a->mMagFilter == b->mMagFilter &&
               a->mMinFilter == b->mMinFilter && a->mMipmapFilter == b->mMipmapFilter &&
               a->mLodMinClamp == b->mLodMinClamp && a->mLodMaxClamp == b->mLodMaxClamp &&
               a->mCompareFunction == b->mCompareFunction && a->mMaxAnisotropy == b->mMaxAnisotropy;
    }

}  // namespace dawn_native
