/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#ifndef TestFontSelector_h
#define TestFontSelector_h

#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontSelector.h"
#include "platform/testing/UnitTestHelpers.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class TestFontSelector : public FontSelector {
public:
    static PassRefPtrWillBeRawPtr<TestFontSelector> create(const String& path)
    {
        RefPtr<SharedBuffer> fontBuffer = testing::readFromFile(path);
        String otsParseMessage;
        return adoptRefWillBeNoop(new TestFontSelector(FontCustomPlatformData::create(
            fontBuffer.get(), otsParseMessage)));
    }

    ~TestFontSelector() override { }

    PassRefPtr<FontData> getFontData(const FontDescription& fontDescription,
        const AtomicString& familyName) override
    {
        FontPlatformData platformData = m_customPlatformData->fontPlatformData(
            fontDescription.effectiveFontSize(),
            fontDescription.isSyntheticBold(),
            fontDescription.isSyntheticItalic(),
            fontDescription.orientation());
        return SimpleFontData::create(platformData, CustomFontData::create());
    }

    void willUseFontData(const FontDescription&, const AtomicString& familyName,
        UChar32) override { }
    void willUseRange(const FontDescription&, const AtomicString& familyName,
        const FontDataRange&) override { };

    unsigned version() const override { return 0; }
    void fontCacheInvalidated() override { }

private:
    TestFontSelector(PassOwnPtr<FontCustomPlatformData> customPlatformData)
    : m_customPlatformData(customPlatformData)
    {
    }

    OwnPtr<FontCustomPlatformData> m_customPlatformData;
};

Font createTestFont(const AtomicString& familyName,
    const String& fontPath, float size = 16.0f)
{
    FontFamily family;
    family.setFamily(familyName);

    FontDescription fontDescription;
    fontDescription.setFamily(family);
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(size);

    Font font(fontDescription);
    font.update(TestFontSelector::create(fontPath));
    return font;
}

} // namespace blink

#endif // TestFontSelector_h
