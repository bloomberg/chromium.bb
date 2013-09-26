/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, Google Inc. All rights reserved.
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

#ifndef FontPlatformDataChromiumWin_h
#define FontPlatformDataChromiumWin_h

#include "config.h"

#include "../ports/SkTypeface_win.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "core/platform/SharedBuffer.h"
#include "core/platform/graphics/FontOrientation.h"
#include "core/platform/graphics/opentype/OpenTypeVerticalData.h"
#include "wtf/Forward.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/StringImpl.h"

#include <usp10.h>

typedef struct HFONT__ *HFONT;

namespace WebCore {

// Return a typeface associated with the hfont, and return its size and
// lfQuality from the hfont's LOGFONT.
PassRefPtr<SkTypeface> CreateTypefaceFromHFont(HFONT, int* size, int* paintTextFlags);

class FontDescription;
class GraphicsContext;

class FontPlatformData {
public:
    // Used for deleted values in the font cache's hash tables. The hash table
    // will create us with this structure, and it will compare other values
    // to this "Deleted" one. It expects the Deleted one to be differentiable
    // from the NULL one (created with the empty constructor), so we can't just
    // set everything to NULL.
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
    // This constructor takes ownership of the HFONT
    FontPlatformData(HFONT, float size, FontOrientation);
    FontPlatformData(float size, bool bold, bool oblique);
    FontPlatformData(const FontPlatformData&);
    FontPlatformData(const FontPlatformData&, float textSize);
    FontPlatformData(SkTypeface*, const char* name, float textSize, bool fakeBold, bool fakeItalic, FontOrientation = Horizontal);

    void setupPaint(SkPaint*, GraphicsContext* = 0) const;

    FontPlatformData& operator=(const FontPlatformData&);

    bool isHashTableDeletedValue() const { return m_isHashTableDeletedValue; }

    ~FontPlatformData();

    bool isFixedPitch() const;
    HFONT hfont() const { return m_font ? m_font->hfont() : 0; }
    float size() const { return m_size; }
    SkTypeface* typeface() const { return m_typeface.get(); }
    int paintTextFlags() const { return m_paintTextFlags; }

    String fontFamilyName() const;

    FontOrientation orientation() const { return m_orientation; }
    void setOrientation(FontOrientation orientation) { m_orientation = orientation; }

    unsigned hash() const
    {
        return m_font ? m_font->hash() : NULL;
    }

    bool operator==(const FontPlatformData& other) const
    {
        return m_font == other.m_font && m_size == other.m_size && m_orientation == other.m_orientation;
    }

#if ENABLE(OPENTYPE_VERTICAL)
    PassRefPtr<OpenTypeVerticalData> verticalData() const;
    PassRefPtr<SharedBuffer> openTypeTable(uint32_t table) const;
#endif

#ifndef NDEBUG
    String description() const;
#endif

    SCRIPT_FONTPROPERTIES* scriptFontProperties() const;
    SCRIPT_CACHE* scriptCache() const { return &m_scriptCache; }

    static bool ensureFontLoaded(HFONT);

private:
    // We refcount the internal HFONT so that FontPlatformData can be
    // efficiently copied. WebKit depends on being able to copy it, and we
    // don't really want to re-create the HFONT.
    class RefCountedHFONT : public RefCounted<RefCountedHFONT> {
    public:
        static PassRefPtr<RefCountedHFONT> create(HFONT hfont)
        {
            return adoptRef(new RefCountedHFONT(hfont));
        }

        ~RefCountedHFONT();

        HFONT hfont() const { return m_hfont; }
        unsigned hash() const
        {
            return StringHasher::hashMemory<sizeof(HFONT)>(&m_hfont);
        }

        bool operator==(const RefCountedHFONT& other) const
        {
            return m_hfont == other.m_hfont;
        }

    private:
        // The create() function assumes there is already a refcount of one
        // so it can do adoptRef.
        RefCountedHFONT(HFONT hfont) : m_hfont(hfont)
        {
        }

        HFONT m_hfont;
    };

    RefPtr<RefCountedHFONT> m_font;
    float m_size;  // Point size of the font in pixels.
    FontOrientation m_orientation;

    RefPtr<SkTypeface> m_typeface; // cached from m_font
    int m_paintTextFlags; // cached from m_font

    mutable SCRIPT_CACHE m_scriptCache;
    mutable OwnPtr<SCRIPT_FONTPROPERTIES> m_scriptFontProperties;

    bool m_isHashTableDeletedValue;
};

} // WebCore

#endif // FontPlatformDataChromiumWin_h
