// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGFontFaceSource_h
#define SVGFontFaceSource_h

#if ENABLE(SVG_FONTS)

#include "core/css/CSSFontFaceSource.h"

namespace blink {

class SVGFontFaceElement;

class SVGFontFaceSource : public CSSFontFaceSource {
public:
    SVGFontFaceSource(SVGFontFaceElement*);

    virtual void trace(Visitor*) override;

private:
    virtual PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override;

    RawPtrWillBeMember<SVGFontFaceElement> m_svgFontFaceElement;
};

} // namespace blink

#endif // ENABLE(SVG_FONTS)
#endif
