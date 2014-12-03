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

#ifndef Picture_h
#define Picture_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutPoint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/FastAllocBase.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class IntSize;

class PLATFORM_EXPORT Picture final : public WTF::RefCounted<Picture> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Picture);
public:
    static PassRefPtr<Picture> create(const FloatRect& bounds)
    {
        return adoptRef(new Picture(bounds));
    }

    virtual ~Picture() { }

    const FloatRect& bounds() const { return m_bounds; }

    // This entry point will return 0 when the PictureRecorder is in the midst of
    // recording (i.e., between a GraphicsContext beginRecording/endRecording pair)
    // and if no recording has ever been completed. Otherwise it will return
    // the picture created by the last endRecording call.
    PassRefPtr<SkPicture> skPicture() const { return m_picture; }
    void setSkPicture(SkPicture* skPicture) { m_picture = adoptRef(skPicture); }

private:
    Picture(const FloatRect& bounds) : m_bounds(bounds) { }

    FloatRect m_bounds;
    RefPtr<SkPicture> m_picture;
};

} // namespace blink

#endif // Picture_h
