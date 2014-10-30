/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#ifndef NativeImageSkia_h
#define NativeImageSkia_h

#include "platform/PlatformExport.h"
#include "platform/graphics/GraphicsTypes.h"
#include "public/platform/WebBlendMode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

struct SkRect;

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class IntSize;
class GraphicsContext;

class PLATFORM_EXPORT NativeImageSkia : public RefCounted<NativeImageSkia> {
public:
    static PassRefPtr<NativeImageSkia> create()
    {
        return adoptRef(new NativeImageSkia());
    }

    // This factory method does a shallow copy of the passed-in SkBitmap
    // (ie., it references the same pixel data and bumps the refcount). Use
    // only when you want sharing semantics.
    static PassRefPtr<NativeImageSkia> create(const SkBitmap& bitmap)
    {
        return adoptRef(new NativeImageSkia(bitmap));
    }

    // Returns true if the entire image has been decoded.
    bool isDataComplete() const { return bitmap().isImmutable(); }

    // Get reference to the internal SkBitmap representing this image.
    const SkBitmap& bitmap() const { return m_bitmap; }

    void draw(
        GraphicsContext*,
        const SkRect& srcRect,
        const SkRect& destRect,
        CompositeOperator,
        WebBlendMode) const;

    void drawPattern(
        GraphicsContext*,
        const FloatRect& srcRect,
        const FloatSize& scale,
        const FloatPoint& phase,
        CompositeOperator,
        const FloatRect& destRect,
        WebBlendMode,
        const IntSize& repeatSpacing) const;

private:
    NativeImageSkia() { }
    explicit NativeImageSkia(const SkBitmap& bitmap) : m_bitmap(bitmap) { }

    SkBitmap m_bitmap;
};

} // namespace blink

#endif  // NativeImageSkia_h
