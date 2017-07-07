// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutLocale_h
#define LayoutLocale_h

#include "platform/PlatformExport.h"
#include "platform/text/Hyphenation.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/AtomicString.h"

#include <unicode/uscript.h>

struct hb_language_impl_t;

namespace blink {

class Hyphenation;

enum class LineBreakIteratorMode { kDefault, kNormal, kStrict, kLoose };

class PLATFORM_EXPORT LayoutLocale : public RefCounted<LayoutLocale> {
 public:
  static const LayoutLocale* Get(const AtomicString& locale);
  static const LayoutLocale& GetDefault();
  static const LayoutLocale& GetSystem();
  static const LayoutLocale& ValueOrDefault(const LayoutLocale* locale) {
    return locale ? *locale : GetDefault();
  }

  bool operator==(const LayoutLocale& other) const {
    return string_ == other.string_;
  }
  bool operator!=(const LayoutLocale& other) const {
    return string_ != other.string_;
  }

  const AtomicString& LocaleString() const { return string_; }
  static const AtomicString& LocaleString(const LayoutLocale* locale) {
    return locale ? locale->string_ : g_null_atom;
  }
  operator const AtomicString&() const { return string_; }
  CString Ascii() const { return string_.Ascii(); }

  const hb_language_impl_t* HarfbuzzLanguage() const {
    return harfbuzz_language_;
  }
  const char* LocaleForSkFontMgr() const;
  UScriptCode GetScript() const { return script_; }

  // Disambiguation of the Unified Han Ideographs.
  UScriptCode GetScriptForHan() const;
  bool HasScriptForHan() const;
  static const LayoutLocale* LocaleForHan(const LayoutLocale*);
  const char* LocaleForHanForSkFontMgr() const;

  Hyphenation* GetHyphenation() const;

  AtomicString LocaleWithBreakKeyword(LineBreakIteratorMode) const;

  static PassRefPtr<LayoutLocale> CreateForTesting(const AtomicString&);
  static void SetHyphenationForTesting(const AtomicString&,
                                       PassRefPtr<Hyphenation>);

 private:
  explicit LayoutLocale(const AtomicString&);

  void ComputeScriptForHan() const;

  AtomicString string_;
  mutable CString string_for_sk_font_mgr_;
  mutable RefPtr<Hyphenation> hyphenation_;

  // hb_language_t is defined in hb.h, which not all files can include.
  const hb_language_impl_t* harfbuzz_language_;

  UScriptCode script_;
  mutable UScriptCode script_for_han_;

  mutable unsigned has_script_for_han_ : 1;
  mutable unsigned hyphenation_computed_ : 1;
};

}  // namespace blink

#endif  // LayoutLocale_h
