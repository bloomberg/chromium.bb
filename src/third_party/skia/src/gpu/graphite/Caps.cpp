/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/Caps.h"

#include "include/gpu/graphite/TextureInfo.h"
#include "src/sksl/SkSLUtil.h"

namespace skgpu::graphite {

Caps::Caps() : fShaderCaps(std::make_unique<SkSL::ShaderCaps>()) {}
Caps::~Caps() {}

void Caps::finishInitialization() {
    this->initSkCaps(fShaderCaps.get());
}

bool Caps::isTexturable(const TextureInfo& info) const {
    if (info.numSamples() > 1) {
        return false;
    }
    return this->onIsTexturable(info);
}

bool Caps::areColorTypeAndTextureInfoCompatible(SkColorType ct, const TextureInfo& info) const {
    // TODO: add SkImage::CompressionType handling
    // (can be handled by setting up the colorTypeInfo instead?)

    return SkToBool(this->getColorTypeInfo(ct, info));
}

skgpu::Swizzle Caps::getReadSwizzle(SkColorType ct, const TextureInfo& info) const {
    // TODO: add SkImage::CompressionType handling
    // (can be handled by setting up the colorTypeInfo instead?)

    auto colorTypeInfo = this->getColorTypeInfo(ct, info);
    if (!colorTypeInfo) {
        SkDEBUGFAILF("Illegal color type (%d) and format combination.", static_cast<int>(ct));
        return {};
    }

    return colorTypeInfo->fReadSwizzle;
}

skgpu::Swizzle Caps::getWriteSwizzle(SkColorType ct, const TextureInfo& info) const {
    auto colorTypeInfo = this->getColorTypeInfo(ct, info);
    if (!colorTypeInfo) {
        SkDEBUGFAILF("Illegal color type (%d) and format combination.", static_cast<int>(ct));
        return {};
    }

    return colorTypeInfo->fWriteSwizzle;
}

} // namespace skgpu::graphite
