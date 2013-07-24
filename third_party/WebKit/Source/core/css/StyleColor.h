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

#ifndef StyleColor_h
#define StyleColor_h

#include "core/platform/graphics/Color.h"
#include "wtf/FastAllocBase.h"

namespace WebCore {

class StyleColor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StyleColor()
        : m_color()
        , m_valid(false)
        , m_currentColor(false) { }
    StyleColor(RGBA32 color)
        : m_color(color)
        , m_valid(true)
        , m_currentColor(false) { }
    StyleColor(Color color, bool valid = true, bool currentColor = false)
        : m_color(color)
        , m_valid(valid)
        , m_currentColor(currentColor) { }
    StyleColor(int r, int g, int b)
        : m_color(Color(r, g, b))
        , m_valid(true)
        , m_currentColor(false) { }
    StyleColor(int r, int g, int b, int a)
        : m_color(Color(r, g, b, a))
        , m_valid(true)
        , m_currentColor(false) { }
    StyleColor(const StyleColor& other)
        : m_color(other.m_color)
        , m_valid(other.m_valid)
        , m_currentColor(other.m_currentColor) { }
    explicit StyleColor(const String&);
    explicit StyleColor(const char*);

    Color color() const { return m_color; }
    bool isValid() const { return m_valid; }
    bool isCurrentColor() const { return m_currentColor; }
    bool hasAlpha() const { return m_color.hasAlpha(); }

    void setNamedColor(const String&);
    void setRGB(int r, int g, int b)
    {
        m_color.setRGB(r, g, b);
        m_valid = true;
        m_currentColor = false;
    }

    RGBA32 rgb() const { return m_color.rgb(); } // Preserve the alpha.
    int red() const { return m_color.red(); }
    int green() const { return m_color.green(); }
    int blue() const { return m_color.blue(); }
    int alpha() const { return m_color.alpha(); }

    static const StyleColor invalid()
    {
        return StyleColor(false, false);
    }
    static const StyleColor currentColor()
    {
        return StyleColor(true, true);
    }

private:
    StyleColor(bool invalid, bool currentColor)
        : m_color()
        , m_valid(invalid)
        , m_currentColor(currentColor) { }

    Color m_color;
    bool m_valid;
    bool m_currentColor;
};

inline bool operator==(const StyleColor& a, const StyleColor& b)
{
    return a.rgb() == b.rgb() && a.isValid() == b.isValid() && a.isCurrentColor() == b.isCurrentColor();
}

inline bool operator!=(const StyleColor& a, const StyleColor& b)
{
    return !(a == b);
}


} // namespace WebCore

#endif // StyleColor_h
