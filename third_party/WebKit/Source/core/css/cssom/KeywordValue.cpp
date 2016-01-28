// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/KeywordValue.h"

#include "wtf/HashMap.h"

namespace blink {

namespace {

using KeywordTable = HashMap<String, KeywordValue::KeywordValueName>;

KeywordTable createKeywordTable()
{
    KeywordTable table;
    table.set(String("initial"), KeywordValue::KeywordValueName::Initial);
    table.set(String("inherit"), KeywordValue::KeywordValueName::Inherit);
    table.set(String("revert"), KeywordValue::KeywordValueName::Revert);
    table.set(String("unset"), KeywordValue::KeywordValueName::Unset);
    return table;
}

KeywordTable& keywordTable()
{
    DEFINE_STATIC_LOCAL(KeywordTable, keywordTable, (createKeywordTable()));
    return keywordTable;
}

} // namespace

String KeywordValue::cssString() const
{
    return m_keywordValue;
}

const String& KeywordValue::keywordValue() const
{
    return m_keywordValue;
}

PassRefPtrWillBeRawPtr<CSSValue> KeywordValue::toCSSValue() const
{
    switch (keywordTable().get(m_keywordValue)) {
    case Initial:
        return cssValuePool().createExplicitInitialValue();
    case Inherit:
        return cssValuePool().createInheritedValue();
    case Revert:
        return cssValuePool().createIdentifierValue(CSSValueID::CSSValueRevert);
    case Unset:
        return cssValuePool().createUnsetValue();
    default:
        // TODO: Support other keywords
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace blink
