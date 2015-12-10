// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeywordValue_h
#define KeywordValue_h

#include "core/CoreExport.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

class CORE_EXPORT KeywordValue : public StyleValue {
    DEFINE_WRAPPERTYPEINFO();

public:
    enum KeywordValueName {
        Initial, Inherit, Revert, Unset
    };

    static PassRefPtrWillBeRawPtr<KeywordValue> create(const String& keyword)
    {
        return adoptRefWillBeNoop(new KeywordValue(keyword));
    }

    StyleValueType type() const override { return KeywordValueType; }

    virtual const String& keywordValue() const;

    String cssString() const override;
    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

protected:
    KeywordValue(const String& keyword) : m_keywordValue(keywordValueFromString(keyword)) {}

    static KeywordValueName keywordValueFromString(const String& keyword);

    KeywordValueName m_keywordValue;
};

}

#endif
