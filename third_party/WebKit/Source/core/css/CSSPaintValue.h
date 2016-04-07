// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintValue_h
#define CSSPaintValue_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintValue : public CSSImageGeneratorValue {
public:
    static CSSPaintValue* create(CSSCustomIdentValue* name)
    {
        return new CSSPaintValue(name);
    }
    ~CSSPaintValue();

    String customCSSText() const;

    String name() const;

    PassRefPtr<Image> image(const LayoutObject&, const IntSize&);
    bool isFixedSize() const { return false; }
    IntSize fixedSize(const LayoutObject&) { return IntSize(); }

    bool isPending() const { return false; }
    bool knownToBeOpaque(const LayoutObject&) const { return false; }

    void loadSubimages(Document*) { }

    bool equals(const CSSPaintValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    explicit CSSPaintValue(CSSCustomIdentValue* name);

    Member<CSSCustomIdentValue> m_name;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPaintValue, isPaintValue());

} // namespace blink

#endif // CSSPaintValue_h
