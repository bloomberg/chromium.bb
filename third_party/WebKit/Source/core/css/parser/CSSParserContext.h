// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserContext_h
#define CSSParserContext_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"

namespace blink {

class CSSStyleSheet;
class StyleSheetContents;

class CORE_EXPORT CSSParserContext
    : public GarbageCollectedFinalized<CSSParserContext> {
 public:
  // https://drafts.csswg.org/selectors/#profiles
  enum SelectorProfile { DynamicProfile, StaticProfile };

  // All three of these factories copy the context and override the current
  // Document handle used for UseCounter.
  static CSSParserContext* createWithStyleSheet(const CSSParserContext*,
                                                const CSSStyleSheet*);
  static CSSParserContext* createWithStyleSheetContents(
      const CSSParserContext*,
      const StyleSheetContents*);
  // FIXME: This constructor shouldn't exist if we properly piped the UseCounter
  // through the CSS subsystem. Currently the UseCounter life time is too crazy
  // and we need a way to override it.
  static CSSParserContext* create(const CSSParserContext* other,
                                  const Document* useCounterDocument);

  static CSSParserContext* create(CSSParserMode,
                                  SelectorProfile = DynamicProfile,
                                  const Document* useCounterDocument = nullptr);
  static CSSParserContext* create(const Document&,
                                  const Document* useCounterDocument);
  static CSSParserContext* create(const Document&,
                                  const KURL& baseURLOverride = KURL(),
                                  const String& charset = emptyString,
                                  SelectorProfile = DynamicProfile,
                                  const Document* useCounterDocument = nullptr);

  bool operator==(const CSSParserContext&) const;
  bool operator!=(const CSSParserContext& other) const {
    return !(*this == other);
  }

  CSSParserMode mode() const { return m_mode; }
  CSSParserMode matchMode() const { return m_matchMode; }
  const KURL& baseURL() const { return m_baseURL; }
  const String& charset() const { return m_charset; }
  const Referrer& referrer() const { return m_referrer; }
  bool isHTMLDocument() const { return m_isHTMLDocument; }
  bool isDynamicProfile() const { return m_profile == DynamicProfile; }
  bool isStaticProfile() const { return m_profile == StaticProfile; }

  // This quirk is to maintain compatibility with Android apps built on
  // the Android SDK prior to and including version 18. Presumably, this
  // can be removed any time after 2015. See http://crbug.com/277157.
  bool useLegacyBackgroundSizeShorthandBehavior() const {
    return m_useLegacyBackgroundSizeShorthandBehavior;
  }

  // FIXME: These setters shouldn't exist, however the current lifetime of
  // CSSParserContext is not well understood and thus we sometimes need to
  // override these fields.
  void setMode(CSSParserMode mode) { m_mode = mode; }
  void setBaseURL(const KURL& baseURL) { m_baseURL = baseURL; }
  void setCharset(const String& charset) { m_charset = charset; }
  void setReferrer(const Referrer& referrer) { m_referrer = referrer; }

  KURL completeURL(const String& url) const;

  void count(UseCounter::Feature) const;
  void count(CSSParserMode, CSSPropertyID) const;
  void countDeprecation(UseCounter::Feature) const;
  bool isUseCounterRecordingEnabled() const { return m_document; }
  bool isDocumentHandleEqual(const Document* other) const;

  ContentSecurityPolicyDisposition shouldCheckContentSecurityPolicy() const {
    return m_shouldCheckContentSecurityPolicy;
  }

  DECLARE_TRACE();

 private:
  CSSParserContext(const KURL& baseURL,
                   const String& charset,
                   CSSParserMode,
                   CSSParserMode matchMode,
                   SelectorProfile,
                   const Referrer&,
                   bool isHTMLDocument,
                   bool useLegacyBackgroundSizeShorthandBehavior,
                   ContentSecurityPolicyDisposition,
                   const Document* useCounterDocument);

  KURL m_baseURL;
  String m_charset;
  CSSParserMode m_mode;
  CSSParserMode m_matchMode;
  SelectorProfile m_profile = DynamicProfile;
  Referrer m_referrer;
  bool m_isHTMLDocument;
  bool m_useLegacyBackgroundSizeShorthandBehavior;
  ContentSecurityPolicyDisposition m_shouldCheckContentSecurityPolicy;

  WeakMember<const Document> m_document;
};

CORE_EXPORT const CSSParserContext* strictCSSParserContext();

}  // namespace blink

#endif  // CSSParserContext_h
