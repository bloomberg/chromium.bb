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
#include "core/platform/graphics/FontCustomPlatformData.h"

#include "core/platform/LayoutTestSupport.h"
#include "core/platform/NotImplemented.h"
#include "platform/SharedBuffer.h"
#include "core/platform/graphics/FontPlatformData.h"
#include "core/platform/graphics/opentype/OpenTypeSanitizer.h"
#include "core/platform/graphics/opentype/OpenTypeUtilities.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/Base64.h"

#include <objbase.h>

namespace {

// Creates a unique and unpredictable font name, in order to avoid collisions and to
// not allow access from CSS.
String createUniqueFontName()
{
    GUID fontUuid;
    CoCreateGuid(&fontUuid);

    String fontName = base64Encode(reinterpret_cast<char*>(&fontUuid), sizeof(fontUuid));
    ASSERT(fontName.length() < LF_FACESIZE);
    return fontName;
}

} // namespace

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(HANDLE fontReference, const String& name)
    : m_fontReference(fontReference)
    , m_name(name)
{
}

FontCustomPlatformData::~FontCustomPlatformData()
{
    if (m_fontReference)
        RemoveFontMemResourceEx(m_fontReference);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant)
{
    ASSERT(m_fontReference);

    LOGFONT logFont;
    // m_name comes from createUniqueFontName, which, in turn, gets
    // it from base64-encoded uuid (128-bit). So, m_name
    // can never be longer than LF_FACESIZE (32).
    if (m_name.length() + 1 >= LF_FACESIZE) {
        ASSERT_NOT_REACHED();
        return FontPlatformData();
    }
    unsigned len = m_name.copyTo(logFont.lfFaceName, 0, LF_FACESIZE - 1);
    logFont.lfFaceName[len] = '\0';

    // FIXME: almost identical to FillLogFont in FontCacheWin.cpp.
    // Need to refactor.
    logFont.lfHeight = -size;
    logFont.lfWidth = 0;
    logFont.lfEscapement = 0;
    logFont.lfOrientation = 0;
    logFont.lfUnderline = false;
    logFont.lfStrikeOut = false;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    logFont.lfQuality = isRunningLayoutTest() ? NONANTIALIASED_QUALITY : DEFAULT_QUALITY; // Honor user's desktop settings.
    logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    logFont.lfItalic = italic;
    logFont.lfWeight = bold ? FW_BOLD : FW_DONTCARE;

    HFONT hfont = CreateFontIndirect(&logFont);
    return FontPlatformData(hfont, size, orientation);
}

PassOwnPtr<FontCustomPlatformData> FontCustomPlatformData::create(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

    OpenTypeSanitizer sanitizer(buffer);
    RefPtr<SharedBuffer> transcodeBuffer = sanitizer.sanitize();
    if (!transcodeBuffer)
        return nullptr; // validation failed.
    buffer = transcodeBuffer.get();

    // Introduce the font to GDI. AddFontMemResourceEx should be used with care, because it will pollute the process's
    // font namespace (Windows has no API for creating an HFONT from data without exposing the font to the
    // entire process first).
    String fontName = createUniqueFontName();
    HANDLE fontReference = renameAndActivateFont(buffer, fontName);
    if (!fontReference)
        return nullptr;

    return adoptPtr(new FontCustomPlatformData(fontReference, fontName));
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || OpenTypeSanitizer::supportsFormat(format);
}

} // namespace WebCore
