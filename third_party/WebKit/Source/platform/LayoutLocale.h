// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutLocale_h
#define LayoutLocale_h

#include "platform/PlatformExport.h"
#include "platform/text/Hyphenation.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicString.h"

#include <unicode/uscript.h>

struct hb_language_impl_t;

namespace blink {

class Hyphenation;

class PLATFORM_EXPORT LayoutLocale : public RefCounted<LayoutLocale> {
public:
    static const LayoutLocale* get(const AtomicString& locale);
    static const LayoutLocale& getDefault();
    static const LayoutLocale& valueOrDefault(const LayoutLocale* locale) { return locale ? *locale : getDefault(); }

    const AtomicString& localeString() const { return m_string; }
    static const AtomicString& localeString(const LayoutLocale* locale) { return locale ? locale->m_string : nullAtom; }
    operator const AtomicString&() const { return m_string; }
    CString ascii() const { return m_string.ascii(); }

    const hb_language_impl_t* harfbuzzLanguage() const { return m_harfbuzzLanguage; }
    const CString& localeForSkFontMgr() const;
    UScriptCode script() const { return m_script; }
    UScriptCode scriptForHan() const;

    Hyphenation* getHyphenation() const;

    static void clearForTesting();
    static void setHyphenationForTesting(const AtomicString&, PassRefPtr<Hyphenation>);

private:
    explicit LayoutLocale(const AtomicString&);

    AtomicString m_string;
    mutable CString m_stringForSkFontMgr;
    mutable RefPtr<Hyphenation> m_hyphenation;

    // hb_language_t is defined in hb.h, which not all files can include.
    const hb_language_impl_t* m_harfbuzzLanguage;

    UScriptCode m_script;
    mutable UScriptCode m_scriptForHan;

    mutable unsigned m_hyphenationComputed : 1;

    static const LayoutLocale* s_default;
};

} // namespace blink

#endif // LayoutLocale_h
