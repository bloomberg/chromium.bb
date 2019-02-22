/*
 * Copyright 2018 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrCoverageCountingPathRenderer.h"

bool GrCoverageCountingPathRenderer::IsSupported(const GrCaps& caps) {
    return false;
}

sk_sp<GrCoverageCountingPathRenderer> GrCoverageCountingPathRenderer::CreateIfSupported(
        const GrCaps& caps, AllowCaching allowCaching) {
    return nullptr;
}

std::unique_ptr<GrFragmentProcessor> GrCoverageCountingPathRenderer::makeClipProcessor(
        uint32_t opListID, const SkPath& deviceSpacePath, const SkIRect& accessRect, int rtWidth,
        int rtHeight, const GrCaps& caps) {
    return nullptr;
}
