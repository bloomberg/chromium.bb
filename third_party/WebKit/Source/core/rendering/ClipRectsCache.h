/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ClipRectsCache_h
#define ClipRectsCache_h

#include "core/rendering/ClipRects.h"

#ifndef NDEBUG
#include "core/rendering/RenderBox.h" // For OverlayScrollbarSizeRelevancy.
#endif

namespace WebCore {

enum ClipRectsType {
    PaintingClipRects, // Relative to painting ancestor. Used for painting.
    RootRelativeClipRects, // Relative to the ancestor treated as the root (e.g. transformed layer). Used for hit testing.
    AbsoluteClipRects, // Relative to the RenderView's layer. Used for compositing overlap testing.
    NumCachedClipRectsTypes,
    AllClipRectTypes,
    TemporaryClipRects
};

enum ShouldRespectOverflowClip {
    IgnoreOverflowClip,
    RespectOverflowClip
};

struct ClipRectsCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipRectsCache()
    {
        for (int i = 0; i < NumCachedClipRectsTypes; ++i) {
            m_clipRectsRoot[i] = 0;
#ifndef NDEBUG
            m_scrollbarRelevancy[i] = IgnoreOverlayScrollbarSize;
#endif
        }
    }

    ClipRects* getClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow) { return m_clipRects[getIndex(clipRectsType, respectOverflow)].get(); }
    void setClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow, PassRefPtr<ClipRects> clipRects, const RenderLayer* root)
    {
        m_clipRects[getIndex(clipRectsType, respectOverflow)] = clipRects;
        m_clipRectsRoot[clipRectsType] = root;
    }

    const RenderLayer* clipRectsRoot(ClipRectsType clipRectsType) const { return m_clipRectsRoot[clipRectsType]; }

#ifndef NDEBUG
    OverlayScrollbarSizeRelevancy m_scrollbarRelevancy[NumCachedClipRectsTypes];
#endif

private:
    int getIndex(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow)
    {
        int index = static_cast<int>(clipRectsType);
        if (respectOverflow == RespectOverflowClip)
            index += static_cast<int>(NumCachedClipRectsTypes);
        return index;
    }

    const RenderLayer* m_clipRectsRoot[NumCachedClipRectsTypes];
    RefPtr<ClipRects> m_clipRects[NumCachedClipRectsTypes * 2];
};

} // namespace WebCore

#endif // ClipRectsCache_h
