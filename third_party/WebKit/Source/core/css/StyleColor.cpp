/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "core/css/StyleColor.h"

#include "core/platform/HashTools.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

StyleColor::StyleColor(const String& name)
{
    if (name[0] == '#') {
        RGBA32 rgba;
        if (name.is8Bit())
            m_valid = Color::parseHexColor(name.characters8() + 1, name.length() - 1, rgba);
        else
            m_valid = Color::parseHexColor(name.characters16() + 1, name.length() - 1, rgba);
        m_color = rgba;
    } else {
        setNamedColor(name);
    }
}

StyleColor::StyleColor(const char* name)
{
    if (name[0] == '#') {
        RGBA32 rgba;
        m_valid = Color::parseHexColor(&name[1], rgba);
        m_color = rgba;
    } else {
        const NamedColor* foundColor = findColor(name, strlen(name));
        m_color = foundColor ? foundColor->ARGBValue : 0;
        m_valid = foundColor;
    }
}

static inline const NamedColor* findNamedColor(const String& name)
{
    char buffer[64]; // easily big enough for the longest color name
    unsigned length = name.length();
    if (length > sizeof(buffer) - 1)
        return 0;
    for (unsigned i = 0; i < length; ++i) {
        UChar c = name[i];
        if (!c || c > 0x7F)
            return 0;
        buffer[i] = toASCIILower(static_cast<char>(c));
    }
    buffer[length] = '\0';
    return findColor(buffer, length);
}

void StyleColor::setNamedColor(const String& name)
{
    const NamedColor* foundColor = findNamedColor(name);
    m_color = foundColor ? foundColor->ARGBValue : 0;
    m_valid = foundColor;
}

} // namespace WebCore
