/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/style/ShadowList.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace WebCore {

static inline void calculateShadowExtent(const ShadowList* shadowList, int additionalOutlineSize, int& shadowLeft, int& shadowRight, int& shadowTop, int& shadowBottom)
{
    ASSERT(shadowList);
    size_t shadowCount = shadowList->shadows().size();
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& shadow = shadowList->shadows()[i];
        if (shadow.style() == Inset)
            continue;
        int blurAndSpread = shadow.blur() + shadow.spread() + additionalOutlineSize;
        shadowLeft = std::min(shadow.x() - blurAndSpread, shadowLeft);
        shadowRight = std::max(shadow.x() + blurAndSpread, shadowRight);
        shadowTop = std::min(shadow.y() - blurAndSpread, shadowTop);
        shadowBottom = std::max(shadow.y() + blurAndSpread, shadowBottom);
    }
}

void ShadowList::adjustRectForShadow(LayoutRect& rect, int additionalOutlineSize) const
{
    int shadowLeft = 0;
    int shadowRight = 0;
    int shadowTop = 0;
    int shadowBottom = 0;
    calculateShadowExtent(this, additionalOutlineSize, shadowLeft, shadowRight, shadowTop, shadowBottom);

    rect.move(shadowLeft, shadowTop);
    rect.setWidth(rect.width() - shadowLeft + shadowRight);
    rect.setHeight(rect.height() - shadowTop + shadowBottom);
}

void ShadowList::adjustRectForShadow(FloatRect& rect, int additionalOutlineSize) const
{
    int shadowLeft = 0;
    int shadowRight = 0;
    int shadowTop = 0;
    int shadowBottom = 0;
    calculateShadowExtent(this, additionalOutlineSize, shadowLeft, shadowRight, shadowTop, shadowBottom);

    rect.move(shadowLeft, shadowTop);
    rect.setWidth(rect.width() - shadowLeft + shadowRight);
    rect.setHeight(rect.height() - shadowTop + shadowBottom);
}

} // namespace WebCore
