/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef CustomFontData_h
#define CustomFontData_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include <unicode/uchar.h>

namespace WebCore {

class CSSFontFaceSource;
struct GlyphData;
class GlyphPage;
class SimpleFontData;
struct WidthIterator;

class PLATFORM_EXPORT CustomFontData : public RefCounted<CustomFontData> {
public:
    enum FallbackVisibility { InvisibleFallback, VisibleFallback };
    static PassRefPtr<CustomFontData> create(bool isLoadingFallback = false, FallbackVisibility visibility = VisibleFallback)
    {
        return adoptRef(new CustomFontData(isLoadingFallback, visibility));
    }

    virtual ~CustomFontData() { }

    virtual void beginLoadIfNeeded() const { };
    bool isLoading() const { return m_isLoadingFallback && m_isUsed; }
    bool isLoadingFallback() const { return m_isLoadingFallback; }
    virtual bool shouldSkipDrawing() const { return false; }

    virtual bool isSVGFont() const { return false; }
    virtual void initializeFontData(SimpleFontData*, float) { }
    virtual float widthForSVGGlyph(Glyph, float) const { return 0.0f; }
    virtual bool fillSVGGlyphPage(GlyphPage*, unsigned, unsigned, UChar*, unsigned, const SimpleFontData*) const { return false; }
    virtual bool applySVGGlyphSelection(WidthIterator&, GlyphData&, bool, int, unsigned&) const { return false; }

    virtual void setCSSFontFaceSource(CSSFontFaceSource* source) { ASSERT_NOT_REACHED(); }
    virtual void clearCSSFontFaceSource() { }

protected:
    CustomFontData(bool isLoadingFallback, FallbackVisibility visibility = VisibleFallback)
        : m_isLoadingFallback(isLoadingFallback)
        , m_fallbackVisibility(visibility)
        , m_isUsed(false)
    {
    }

    bool m_isLoadingFallback;
    FallbackVisibility m_fallbackVisibility;
    mutable bool m_isUsed;
};

}

#endif // CustomFontData_h
