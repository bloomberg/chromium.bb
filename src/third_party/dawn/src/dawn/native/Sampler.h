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

#ifndef SRC_DAWN_NATIVE_SAMPLER_H_
#define SRC_DAWN_NATIVE_SAMPLER_H_

#include "dawn/native/CachedObject.h"
#include "dawn/native/Error.h"
#include "dawn/native/Forward.h"
#include "dawn/native/ObjectBase.h"

#include "dawn/native/dawn_platform.h"

namespace dawn::native {

class DeviceBase;

MaybeError ValidateSamplerDescriptor(DeviceBase* device, const SamplerDescriptor* descriptor);

class SamplerBase : public ApiObjectBase, public CachedObject {
  public:
    SamplerBase(DeviceBase* device,
                const SamplerDescriptor* descriptor,
                ApiObjectBase::UntrackedByDeviceTag tag);
    SamplerBase(DeviceBase* device, const SamplerDescriptor* descriptor);
    ~SamplerBase() override;

    static SamplerBase* MakeError(DeviceBase* device);

    ObjectType GetType() const override;

    bool IsComparison() const;
    bool IsFiltering() const;

    // Functions necessary for the unordered_set<SamplerBase*>-based cache.
    size_t ComputeContentHash() override;

    struct EqualityFunc {
        bool operator()(const SamplerBase* a, const SamplerBase* b) const;
    };

    uint16_t GetMaxAnisotropy() const { return mMaxAnisotropy; }

  protected:
    // Constructor used only for mocking and testing.
    explicit SamplerBase(DeviceBase* device);
    void DestroyImpl() override;

  private:
    SamplerBase(DeviceBase* device, ObjectBase::ErrorTag tag);

    // TODO(cwallez@chromium.org): Store a crypto hash of the items instead?
    wgpu::AddressMode mAddressModeU;
    wgpu::AddressMode mAddressModeV;
    wgpu::AddressMode mAddressModeW;
    wgpu::FilterMode mMagFilter;
    wgpu::FilterMode mMinFilter;
    wgpu::FilterMode mMipmapFilter;
    float mLodMinClamp;
    float mLodMaxClamp;
    wgpu::CompareFunction mCompareFunction;
    uint16_t mMaxAnisotropy;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_SAMPLER_H_
