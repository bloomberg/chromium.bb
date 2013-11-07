/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
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
 *     * Neither the name of Opera Software ASA nor the names of its
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

#ifndef BorderImageLengthBox_h
#define BorderImageLengthBox_h

#include "core/rendering/style/BorderImageLength.h"
#include "platform/LengthBox.h"

namespace WebCore {

// Represents a computed border image width or outset.
//
// http://www.w3.org/TR/css3-background/#border-image-width
// http://www.w3.org/TR/css3-background/#border-image-outset
class BorderImageLengthBox {
public:
    BorderImageLengthBox()
    {
    }

    BorderImageLengthBox(Length top, Length right, Length bottom, Length left)
        : m_left(left)
        , m_right(right)
        , m_top(top)
        , m_bottom(bottom)
    {
    }

    BorderImageLengthBox(double top, double right, double bottom, double left)
        : m_left(left)
        , m_right(right)
        , m_top(top)
        , m_bottom(bottom)
    {
    }

    // FIXME: Remove when the conversion to BorderImageLengthBox is complete.
    explicit BorderImageLengthBox(LengthBox lengthBox)
        : m_left(lengthBox.m_left)
        , m_right(lengthBox.m_right)
        , m_top(lengthBox.m_top)
        , m_bottom(lengthBox.m_bottom)
    {
    }

    const BorderImageLength& left() const { return m_left; }
    const BorderImageLength& right() const { return m_right; }
    const BorderImageLength& top() const { return m_top; }
    const BorderImageLength& bottom() const { return m_bottom; }

    void setLeft(const BorderImageLength& left) { m_left = left; }
    void setRight(const BorderImageLength& right) { m_right = right; }
    void setTop(const BorderImageLength& top) { m_top = top; }
    void setBottom(const BorderImageLength& bottom) { m_bottom = bottom; }

    bool operator==(const BorderImageLengthBox& other) const
    {
        return m_left == other.m_left && m_right == other.m_right
            && m_top == other.m_top && m_bottom == other.m_bottom;
    }

    bool operator!=(const BorderImageLengthBox& other) const
    {
        return !(*this == other);
    }

    bool nonZero() const
    {
        return !(m_left.isZero() && m_right.isZero() && m_top.isZero() && m_bottom.isZero());
    }

    // FIXME: Remove when the conversion to BorderImageLengthBox is complete.
    LengthBox deprecatedLengthBox() const
    {
        return LengthBox(m_top.deprecatedUnifiedLength(), m_right.deprecatedUnifiedLength(),
            m_bottom.deprecatedUnifiedLength(), m_left.deprecatedUnifiedLength());
    }

private:
    BorderImageLength m_left;
    BorderImageLength m_right;
    BorderImageLength m_top;
    BorderImageLength m_bottom;
};

} // namespace WebCore

#endif // BorderImageLengthBox_h
