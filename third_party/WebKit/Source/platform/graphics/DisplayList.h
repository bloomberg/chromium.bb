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

#ifndef DisplayList_h
#define DisplayList_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutPoint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/FastAllocBase.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class IntSize;

class PLATFORM_EXPORT DisplayList final : public WTF::RefCounted<DisplayList> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DisplayList);
public:
    static PassRefPtr<DisplayList> create(const FloatRect& bounds)
    {
        return adoptRef(new DisplayList(bounds));
    }

    virtual ~DisplayList() { }

    const FloatRect& bounds() const { return m_bounds; }

    // This entry point will return 0 when the DisplayList is in the midst of
    // recording (i.e., between a GraphicsContext beginRecording/endRecording pair)
    // and if no recording has ever been completed. Otherwise it will return
    // the picture created by the last endRecording call.
    SkPicture* picture() const { return m_picture.get(); }
    void setPicture(SkPicture* picture) { m_picture = adoptRef(picture); }

    // FIXME: Need unit testing of these methods and their effect
    const SkMatrix& transform() const { return m_transform; }
    void setTransform(const SkMatrix& transform) { m_transform = transform; }
    void setTransformFromPaintOffset(const LayoutPoint& paintOffset)
    {
        SkMatrix m;
        m.setTranslate(paintOffset.x().toFloat(), paintOffset.y().toFloat());
        setTransform(m);
    }
    void clearTransform() { m_transform.reset(); }

    // FIXME: Need unit testing of these methods and their effect
    const SkRect& clip() const { return m_clip; }
    void setClip(const IntRect& rect) { m_clip = rect; }
    void setClip(const FloatRect& rect) { m_clip = rect; }
    void clearClip() { m_clip.setEmpty(); }

private:
    DisplayList(const FloatRect& bounds) : m_bounds(bounds)
    {
        clearTransform();
        clearClip();
    }

    FloatRect m_bounds;
    SkMatrix m_transform;
    SkRect m_clip; // TODO: Do we need to support other types of clips here?
    RefPtr<SkPicture> m_picture;
};

} // namespace blink

#endif // DisplayList_h
