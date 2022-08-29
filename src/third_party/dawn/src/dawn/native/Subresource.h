// Copyright 2020 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_SUBRESOURCE_H_
#define SRC_DAWN_NATIVE_SUBRESOURCE_H_

#include "dawn/native/EnumClassBitmasks.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

// Note: Subresource indices are computed by iterating the aspects in increasing order.
// D3D12 uses these directly, so the order much match D3D12's indices.
//  - Depth/Stencil textures have Depth as Plane 0, and Stencil as Plane 1.
enum class Aspect : uint8_t {
    None = 0x0,
    Color = 0x1,
    Depth = 0x2,
    Stencil = 0x4,

    // Aspects used to select individual planes in a multi-planar format.
    Plane0 = 0x8,
    Plane1 = 0x10,

    // An aspect for that represents the combination of both the depth and stencil aspects. It
    // can be ignored outside of the Vulkan backend.
    CombinedDepthStencil = 0x20,
};

template <>
struct EnumBitmaskSize<Aspect> {
    static constexpr unsigned value = 6;
};

// Convert the TextureAspect to an Aspect mask for the format. ASSERTs if the aspect
// does not exist in the format.
// Also ASSERTs if "All" is selected and results in more than one aspect.
Aspect ConvertSingleAspect(const Format& format, wgpu::TextureAspect aspect);

// Convert the TextureAspect to an Aspect mask for the format. ASSERTs if the aspect
// does not exist in the format.
Aspect ConvertAspect(const Format& format, wgpu::TextureAspect aspect);

// Returns the Aspects of the Format that are selected by the wgpu::TextureAspect.
// Note that this can return Aspect::None if the Format doesn't have any of the
// selected aspects.
Aspect SelectFormatAspects(const Format& format, wgpu::TextureAspect aspect);

// Convert TextureAspect to the aspect which corresponds to the view format. This
// special cases per plane view formats before calling ConvertAspect.
Aspect ConvertViewAspect(const Format& format, wgpu::TextureAspect aspect);

// Helper struct to make it clear that what the parameters of a range mean.
template <typename T>
struct FirstAndCountRange {
    T first;
    T count;
};

struct SubresourceRange {
    SubresourceRange(Aspect aspects,
                     FirstAndCountRange<uint32_t> arrayLayerParam,
                     FirstAndCountRange<uint32_t> mipLevelParams);
    SubresourceRange();

    Aspect aspects;
    uint32_t baseArrayLayer;
    uint32_t layerCount;
    uint32_t baseMipLevel;
    uint32_t levelCount;

    static SubresourceRange SingleMipAndLayer(uint32_t baseMipLevel,
                                              uint32_t baseArrayLayer,
                                              Aspect aspects);
    static SubresourceRange MakeSingle(Aspect aspect,
                                       uint32_t baseArrayLayer,
                                       uint32_t baseMipLevel);

    static SubresourceRange MakeFull(Aspect aspects, uint32_t layerCount, uint32_t levelCount);
};

// Helper function to use aspects as linear indices in arrays.
uint8_t GetAspectIndex(Aspect aspect);
uint8_t GetAspectCount(Aspect aspects);

// The maximum number of planes per format Dawn knows about. Asserts in BuildFormatTable that
// the per plane index does not exceed the known maximum plane count.
static constexpr uint32_t kMaxPlanesPerFormat = 3;

}  // namespace dawn::native

namespace dawn {

template <>
struct IsDawnBitmask<dawn::native::Aspect> {
    static constexpr bool enable = true;
};

}  // namespace dawn

#endif  // SRC_DAWN_NATIVE_SUBRESOURCE_H_
