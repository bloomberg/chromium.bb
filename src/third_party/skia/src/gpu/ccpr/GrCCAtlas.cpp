/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ccpr/GrCCAtlas.h"

#include "include/private/SkTPin.h"
#include "src/core/SkIPoint16.h"
#include "src/gpu/GrOnFlushResourceProvider.h"

static SkISize choose_initial_atlas_size(const GrCCAtlas::Specs& specs) {
    // Begin with the first pow2 dimensions whose area is theoretically large enough to contain the
    // pending paths, favoring height over width if necessary.
    int log2area = SkNextLog2(std::max(specs.fApproxNumPixels, 1));
    int height = 1 << ((log2area + 1) / 2);
    int width = 1 << (log2area / 2);

    width = SkTPin(width, specs.fMinTextureSize, specs.fMaxPreferredTextureSize);
    height = SkTPin(height, specs.fMinTextureSize, specs.fMaxPreferredTextureSize);

    return SkISize::Make(width, height);
}

static int choose_max_atlas_size(const GrCCAtlas::Specs& specs, const GrCaps& caps) {
    return (std::max(specs.fMinHeight, specs.fMinWidth) <= specs.fMaxPreferredTextureSize) ?
            specs.fMaxPreferredTextureSize : caps.maxRenderTargetSize();
}

GrCCAtlas::GrCCAtlas(const Specs& specs, const GrCaps& caps)
        : GrDynamicAtlas(GrColorType::kAlpha_8, InternalMultisample::kYes,
                         choose_initial_atlas_size(specs), choose_max_atlas_size(specs, caps),
                         caps) {
    SkASSERT(specs.fMaxPreferredTextureSize > 0);
}

GrCCAtlas::~GrCCAtlas() {
}
