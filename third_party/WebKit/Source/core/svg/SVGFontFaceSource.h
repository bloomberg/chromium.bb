// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGFontFaceSource_h
#define SVGFontFaceSource_h

#if ENABLE(SVG_FONTS)

#include "core/css/CSSFontFaceSource.h"

namespace WebCore {

class SVGFontFaceElement;

class SVGFontFaceSource : public CSSFontFaceSource {
public:
    SVGFontFaceSource(SVGFontFaceElement*);

private:
    virtual PassRefPtr<SimpleFontData> createFontData(const FontDescription&) OVERRIDE;

    // This is a raw ptr as the element resides in the same document as the FontFace. This is to avoid a reference cycle.
    // FIXME: Oilpan: This should be a Member when we move SVGFontFaceElement to oilpan.
    SVGFontFaceElement* m_svgFontFaceElement;
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif
