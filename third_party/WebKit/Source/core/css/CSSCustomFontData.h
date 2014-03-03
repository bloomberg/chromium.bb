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

#ifndef CSSCustomFontData_h
#define CSSCustomFontData_h

#include "core/css/CSSFontFaceSource.h"
#include "platform/fonts/CustomFontData.h"

namespace WebCore {

class CSSCustomFontData FINAL : public CustomFontData {
public:
    static PassRefPtr<CSSCustomFontData> create(bool isLoadingFallback = false, FallbackVisibility visibility = VisibleFallback)
    {
        return adoptRef(new CSSCustomFontData(isLoadingFallback, visibility));
    }

    virtual ~CSSCustomFontData() { }

    virtual void beginLoadIfNeeded() const OVERRIDE
    {
        if (!m_isUsed && m_isLoadingFallback && m_fontFaceSource) {
            m_isUsed = true;
            m_fontFaceSource->beginLoadIfNeeded();
        }
    }

    virtual void setCSSFontFaceSource(CSSFontFaceSource* source) OVERRIDE { m_fontFaceSource = source; }
    virtual void clearCSSFontFaceSource() OVERRIDE { m_fontFaceSource = 0; }

private:
    CSSCustomFontData(bool isLoadingFallback, FallbackVisibility visibility)
        : CustomFontData(isLoadingFallback, visibility)
        , m_fontFaceSource(0)
    {
    }

    CSSFontFaceSource* m_fontFaceSource;
};

}

#endif // CSSCustomFontData_h
