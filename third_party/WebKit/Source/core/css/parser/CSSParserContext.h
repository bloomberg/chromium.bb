// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserContext_h
#define CSSParserContext_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/WebFeatureForward.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

class CSSStyleSheet;
class Document;
class StyleSheetContents;

class CORE_EXPORT CSSParserContext
    : public GarbageCollectedFinalized<CSSParserContext> {
 public:
  // https://drafts.csswg.org/selectors/#profiles
  enum SelectorProfile { kDynamicProfile, kStaticProfile };

  // All three of these factories copy the context and override the current
  // Document handle used for UseCounter.
  static CSSParserContext* CreateWithStyleSheet(const CSSParserContext*,
                                                const CSSStyleSheet*);
  static CSSParserContext* CreateWithStyleSheetContents(
      const CSSParserContext*,
      const StyleSheetContents*);
  // FIXME: This constructor shouldn't exist if we properly piped the UseCounter
  // through the CSS subsystem. Currently the UseCounter life time is too crazy
  // and we need a way to override it.
  static CSSParserContext* Create(const CSSParserContext* other,
                                  const Document* use_counter_document);

  static CSSParserContext* Create(const CSSParserContext* other,
                                  const KURL& base_url_override,
                                  ReferrerPolicy referrer_policy_override,
                                  const WTF::TextEncoding& charset_override,
                                  const Document* use_counter_document);

  static CSSParserContext* Create(
      CSSParserMode,
      SecureContextMode,
      SelectorProfile = kDynamicProfile,
      const Document* use_counter_document = nullptr);
  static CSSParserContext* Create(const Document&);
  static CSSParserContext* Create(
      const Document&,
      const KURL& base_url_override,
      ReferrerPolicy referrer_policy_override,
      const WTF::TextEncoding& charset = WTF::TextEncoding(),
      SelectorProfile = kDynamicProfile);
  // This is used for workers, where we don't have a document.
  static CSSParserContext* Create(const ExecutionContext&);

  bool operator==(const CSSParserContext&) const;
  bool operator!=(const CSSParserContext& other) const {
    return !(*this == other);
  }

  CSSParserMode Mode() const { return mode_; }
  CSSParserMode MatchMode() const { return match_mode_; }
  const KURL& BaseURL() const { return base_url_; }
  const WTF::TextEncoding& Charset() const { return charset_; }
  const Referrer& GetReferrer() const { return referrer_; }
  bool IsHTMLDocument() const { return is_html_document_; }
  bool IsDynamicProfile() const { return profile_ == kDynamicProfile; }
  bool IsStaticProfile() const { return profile_ == kStaticProfile; }

  bool IsSecureContext() const;

  // This quirk is to maintain compatibility with Android apps built on
  // the Android SDK prior to and including version 18. Presumably, this
  // can be removed any time after 2015. See http://crbug.com/277157.
  bool UseLegacyBackgroundSizeShorthandBehavior() const {
    return use_legacy_background_size_shorthand_behavior_;
  }

  // FIXME: This setter shouldn't exist, however the current lifetime of
  // CSSParserContext is not well understood and thus we sometimes need to
  // override this field.
  void SetMode(CSSParserMode mode) { mode_ = mode; }

  KURL CompleteURL(const String& url) const;

  SecureContextMode GetSecureContextMode() const {
    return secure_context_mode_;
  }

  void Count(WebFeature) const;
  void Count(CSSParserMode, CSSPropertyID) const;
  void CountDeprecation(WebFeature) const;
  bool IsUseCounterRecordingEnabled() const { return document_; }
  bool IsDocumentHandleEqual(const Document* other) const;

  ContentSecurityPolicyDisposition ShouldCheckContentSecurityPolicy() const {
    return should_check_content_security_policy_;
  }

  void Trace(blink::Visitor*);

 private:
  CSSParserContext(const KURL& base_url,
                   const WTF::TextEncoding& charset,
                   CSSParserMode,
                   CSSParserMode match_mode,
                   SelectorProfile,
                   const Referrer&,
                   bool is_html_document,
                   bool use_legacy_background_size_shorthand_behavior,
                   SecureContextMode,
                   ContentSecurityPolicyDisposition,
                   const Document* use_counter_document);

  KURL base_url_;
  WTF::TextEncoding charset_;
  CSSParserMode mode_;
  CSSParserMode match_mode_;
  SelectorProfile profile_ = kDynamicProfile;
  Referrer referrer_;
  bool is_html_document_;
  bool use_legacy_background_size_shorthand_behavior_;
  SecureContextMode secure_context_mode_;
  ContentSecurityPolicyDisposition should_check_content_security_policy_;

  WeakMember<const Document> document_;
};

CORE_EXPORT const CSSParserContext* StrictCSSParserContext(SecureContextMode);

}  // namespace blink

#endif  // CSSParserContext_h
