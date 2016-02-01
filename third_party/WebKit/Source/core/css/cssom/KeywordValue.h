// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeywordValue_h
#define KeywordValue_h

#include "core/CoreExport.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

class CORE_EXPORT KeywordValue final : public StyleValue {
    WTF_MAKE_NONCOPYABLE(KeywordValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    enum KeywordValueName {
        Initial, Inherit, Revert, Unset
    };

    static KeywordValue* create(const String& keyword)
    {
        return new KeywordValue(keyword);
    }

    StyleValueType type() const override { return KeywordValueType; }

    const String& keywordValue() const;

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

private:
    KeywordValue(const String& keyword) : m_keywordValue(keyword.lower()) {}

    String m_keywordValue;
};

} // namespace blink

#endif
