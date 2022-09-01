/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/mtl/MtlSampler.h"

#include "include/core/SkSamplingOptions.h"
#include "src/gpu/graphite/mtl/MtlCaps.h"
#include "src/gpu/graphite/mtl/MtlGpu.h"

namespace skgpu::graphite {

MtlSampler::MtlSampler(const MtlGpu* gpu,
                       sk_cfp<id<MTLSamplerState>> samplerState)
        : Sampler(gpu)
        , fSamplerState(std::move(samplerState)) {}

static inline MTLSamplerAddressMode tile_mode_to_mtl_sampler_address(SkTileMode tileMode,
                                                                     const Caps& caps) {
    switch (tileMode) {
        case SkTileMode::kClamp:
            return MTLSamplerAddressModeClampToEdge;
        case SkTileMode::kRepeat:
            return MTLSamplerAddressModeRepeat;
        case SkTileMode::kMirror:
            return MTLSamplerAddressModeMirrorRepeat;
        case SkTileMode::kDecal:
            // For this tilemode, we should have checked that clamp-to-border support exists.
            // If it doesn't we should have fallen back to a shader instead.
            // TODO: for textures with alpha, we could use ClampToZero if there's no
            // ClampToBorderColor as they'll clamp to (0,0,0,0).
            // Unfortunately textures without alpha end up clamping to (0,0,0,1).
            if (@available(macOS 10.12, iOS 14.0, *)) {
                SkASSERT(caps.clampToBorderSupport());
                return MTLSamplerAddressModeClampToBorderColor;
            } else {
                SkASSERT(false);
                return MTLSamplerAddressModeClampToZero;
            }
    }
    SkUNREACHABLE;
}

sk_sp<MtlSampler> MtlSampler::Make(const MtlGpu* gpu,
                                   const SkSamplingOptions& samplingOptions,
                                   SkTileMode xTileMode,
                                   SkTileMode yTileMode) {
    sk_cfp<MTLSamplerDescriptor*> desc([[MTLSamplerDescriptor alloc] init]);

    MTLSamplerMinMagFilter minMagFilter = [&] {
        switch (samplingOptions.filter) {
            case SkFilterMode::kNearest: return MTLSamplerMinMagFilterNearest;
            case SkFilterMode::kLinear:  return MTLSamplerMinMagFilterLinear;
        }
        SkUNREACHABLE;
    }();

    MTLSamplerMipFilter mipFilter = [&] {
      switch (samplingOptions.mipmap) {
          case SkMipmapMode::kNone:    return MTLSamplerMipFilterNotMipmapped;
          case SkMipmapMode::kNearest: return MTLSamplerMipFilterNearest;
          case SkMipmapMode::kLinear:  return MTLSamplerMipFilterLinear;
      }
      SkUNREACHABLE;
    }();

    auto samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.sAddressMode = tile_mode_to_mtl_sampler_address(xTileMode, gpu->mtlCaps());
    samplerDesc.tAddressMode = tile_mode_to_mtl_sampler_address(yTileMode, gpu->mtlCaps());
    samplerDesc.magFilter = minMagFilter;
    samplerDesc.minFilter = minMagFilter;
    samplerDesc.mipFilter = mipFilter;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = FLT_MAX;  // default value according to docs.
    samplerDesc.maxAnisotropy = 1.0f;
    samplerDesc.normalizedCoordinates = true;
    if (@available(macOS 10.11, iOS 9.0, *)) {
        samplerDesc.compareFunction = MTLCompareFunctionNever;
    }
#ifdef SK_ENABLE_MTL_DEBUG_INFO
    // TODO: add label?
#endif

    sk_cfp<id<MTLSamplerState>> sampler([gpu->device() newSamplerStateWithDescriptor:desc.get()]);
    if (!sampler) {
        return nullptr;
    }
    return sk_sp<MtlSampler>(new MtlSampler(gpu, std::move(sampler)));
}

void MtlSampler::freeGpuData() {
    fSamplerState.reset();
}

} // namespace skgpu::graphite

