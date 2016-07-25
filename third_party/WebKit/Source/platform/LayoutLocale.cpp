// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LayoutLocale.h"

#include "platform/Language.h"
#include "platform/text/LocaleToScriptMapping.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

#include <hb.h>

namespace blink {

const LayoutLocale* LayoutLocale::s_default = nullptr;

static hb_language_t toHarfbuzLanguage(const AtomicString& locale)
{
    CString localeAsLatin1 = locale.latin1();
    return hb_language_from_string(localeAsLatin1.data(), localeAsLatin1.length());
}

// SkFontMgr requires script-based locale names, like "zh-Hant" and "zh-Hans",
// instead of "zh-CN" and "zh-TW".
static CString toSkFontMgrLocale(const String& locale)
{
    if (!locale.startsWith("zh", TextCaseInsensitive))
        return locale.ascii();

    switch (localeToScriptCodeForFontSelection(locale)) {
    case USCRIPT_SIMPLIFIED_HAN:
        return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
        return "zh-Hant";
    default:
        return locale.ascii();
    }
}

const CString& LayoutLocale::localeForSkFontMgr() const
{
    if (m_stringForSkFontMgr.isNull())
        m_stringForSkFontMgr = toSkFontMgrLocale(m_string);
    return m_stringForSkFontMgr;
}

UScriptCode LayoutLocale::scriptForHan() const
{
    if (m_scriptForHan != USCRIPT_COMMON)
        return m_scriptForHan;

    m_scriptForHan = scriptCodeForHanFromLocale(script(), localeString());
    if (m_scriptForHan == USCRIPT_COMMON)
        m_scriptForHan = USCRIPT_HAN;
    return m_scriptForHan;
}

LayoutLocale::LayoutLocale(const AtomicString& locale)
    : m_string(locale)
    , m_harfbuzzLanguage(toHarfbuzLanguage(locale))
    , m_script(localeToScriptCodeForFontSelection(locale))
    , m_scriptForHan(USCRIPT_COMMON)
    , m_hyphenationComputed(false)
{
}

using LayoutLocaleMap = HashMap<AtomicString, RefPtr<LayoutLocale>, CaseFoldingHash>;

static LayoutLocaleMap& getLocaleMap()
{
    DEFINE_STATIC_LOCAL(LayoutLocaleMap, localeMap, ());
    return localeMap;
}

const LayoutLocale* LayoutLocale::get(const AtomicString& locale)
{
    if (locale.isNull())
        return nullptr;

    auto result = getLocaleMap().add(locale, nullptr);
    if (result.isNewEntry)
        result.storedValue->value = adoptRef(new LayoutLocale(locale));
    return result.storedValue->value.get();
}

static const LayoutLocale* computeDefaultLocale()
{
    AtomicString locale = defaultLanguage();
    if (!locale.isEmpty())
        return LayoutLocale::get(locale);
    return LayoutLocale::get("en");
}

const LayoutLocale& LayoutLocale::getDefault()
{
    if (!s_default)
        s_default = computeDefaultLocale();
    return *s_default;
}

void LayoutLocale::clearForTesting()
{
    s_default = nullptr;
    getLocaleMap().clear();
}

Hyphenation* LayoutLocale::getHyphenation() const
{
    if (m_hyphenationComputed)
        return m_hyphenation.get();

    m_hyphenationComputed = true;
    m_hyphenation = Hyphenation::platformGetHyphenation(localeString());
    return m_hyphenation.get();
}

void LayoutLocale::setHyphenationForTesting(const AtomicString& localeString, PassRefPtr<Hyphenation> hyphenation)
{
    const LayoutLocale& locale = valueOrDefault(get(localeString));
    locale.m_hyphenationComputed = true;
    locale.m_hyphenation = hyphenation;
}

} // namespace blink
