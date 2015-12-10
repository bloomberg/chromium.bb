// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
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

const String& KeywordValue::keywordValue() const
{
    DEFINE_STATIC_LOCAL(const String, InitialStr, ("initial"));
    DEFINE_STATIC_LOCAL(const String, InheritStr, ("inherit"));
    DEFINE_STATIC_LOCAL(const String, RevertStr, ("revert"));
    DEFINE_STATIC_LOCAL(const String, UnsetStr, ("unset"));

    switch (m_keywordValue) {
    case Initial:
        return InitialStr;
    case Inherit:
        return InheritStr;
    case Revert:
        return RevertStr;
    case Unset:
        return UnsetStr;
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

String KeywordValue::cssString() const
{
    return keywordValue();
}

PassRefPtrWillBeRawPtr<CSSValue> KeywordValue::toCSSValue() const
{
    switch (m_keywordValue) {
    case Initial:
        return cssValuePool().createExplicitInitialValue();
    case Inherit:
        return cssValuePool().createInheritedValue();
    case Revert:
        return cssValuePool().createIdentifierValue(CSSValueID::CSSValueRevert);
    case Unset:
        return cssValuePool().createUnsetValue();
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

KeywordValue::KeywordValueName KeywordValue::keywordValueFromString(const String& keyword)
{
    return keywordTable().get(keyword.lower());
}

}
