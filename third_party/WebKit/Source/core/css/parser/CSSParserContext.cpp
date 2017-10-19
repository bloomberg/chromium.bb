// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserContext.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/page/Page.h"

namespace blink {

// static
CSSParserContext* CSSParserContext::Create(const ExecutionContext& context) {
  const Referrer referrer(context.Url().StrippedForUseAsReferrer(),
                          context.GetReferrerPolicy());

  ContentSecurityPolicyDisposition policy_disposition;
  if (ContentSecurityPolicy::ShouldBypassMainWorld(&context))
    policy_disposition = kDoNotCheckContentSecurityPolicy;
  else
    policy_disposition = kCheckContentSecurityPolicy;

  return new CSSParserContext(
      context.Url(), WTF::TextEncoding(), kHTMLStandardMode, kHTMLStandardMode,
      kDynamicProfile, referrer, true, false, policy_disposition,
      context.IsDocument() ? &ToDocument(context) : nullptr);
}

// static
CSSParserContext* CSSParserContext::CreateWithStyleSheet(
    const CSSParserContext* other,
    const CSSStyleSheet* style_sheet) {
  return CSSParserContext::Create(
      other, CSSStyleSheet::SingleOwnerDocument(style_sheet));
}

// static
CSSParserContext* CSSParserContext::CreateWithStyleSheetContents(
    const CSSParserContext* other,
    const StyleSheetContents* style_sheet_contents) {
  return CSSParserContext::Create(
      other, StyleSheetContents::SingleOwnerDocument(style_sheet_contents));
}

// static
CSSParserContext* CSSParserContext::Create(
    const CSSParserContext* other,
    const Document* use_counter_document) {
  return new CSSParserContext(
      other->base_url_, other->charset_, other->mode_, other->match_mode_,
      other->profile_, other->referrer_, other->is_html_document_,
      other->use_legacy_background_size_shorthand_behavior_,
      other->should_check_content_security_policy_, use_counter_document);
}

// static
CSSParserContext* CSSParserContext::Create(
    const CSSParserContext* other,
    const KURL& base_url,
    ReferrerPolicy referrer_policy,
    const WTF::TextEncoding& charset,
    const Document* use_counter_document) {
  return new CSSParserContext(
      base_url, charset, other->mode_, other->match_mode_, other->profile_,
      Referrer(base_url.StrippedForUseAsReferrer(), referrer_policy),
      other->is_html_document_,
      other->use_legacy_background_size_shorthand_behavior_,
      other->should_check_content_security_policy_, use_counter_document);
}

// static
CSSParserContext* CSSParserContext::Create(
    CSSParserMode mode,
    SelectorProfile profile,
    const Document* use_counter_document) {
  return new CSSParserContext(
      KURL(), WTF::TextEncoding(), mode, mode, profile, Referrer(), false,
      false, kDoNotCheckContentSecurityPolicy, use_counter_document);
}

// static
CSSParserContext* CSSParserContext::Create(const Document& document) {
  return CSSParserContext::Create(document, document.BaseURL(),
                                  document.GetReferrerPolicy(),
                                  WTF::TextEncoding(), kDynamicProfile);
}

// static
CSSParserContext* CSSParserContext::Create(
    const Document& document,
    const KURL& base_url_override,
    ReferrerPolicy referrer_policy_override,
    const WTF::TextEncoding& charset,
    SelectorProfile profile) {
  CSSParserMode mode =
      document.InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode;
  CSSParserMode match_mode;
  HTMLImportsController* imports_controller = document.ImportsController();
  if (imports_controller && profile == kDynamicProfile) {
    match_mode = imports_controller->Master()->InQuirksMode()
                     ? kHTMLQuirksMode
                     : kHTMLStandardMode;
  } else {
    match_mode = mode;
  }

  const Referrer referrer(base_url_override.StrippedForUseAsReferrer(),
                          referrer_policy_override);

  bool use_legacy_background_size_shorthand_behavior =
      document.GetSettings()
          ? document.GetSettings()
                ->GetUseLegacyBackgroundSizeShorthandBehavior()
          : false;

  ContentSecurityPolicyDisposition policy_disposition;
  if (ContentSecurityPolicy::ShouldBypassMainWorld(&document))
    policy_disposition = kDoNotCheckContentSecurityPolicy;
  else
    policy_disposition = kCheckContentSecurityPolicy;

  return new CSSParserContext(base_url_override, charset, mode, match_mode,
                              profile, referrer, document.IsHTMLDocument(),
                              use_legacy_background_size_shorthand_behavior,
                              policy_disposition, &document);
}

CSSParserContext::CSSParserContext(
    const KURL& base_url,
    const WTF::TextEncoding& charset,
    CSSParserMode mode,
    CSSParserMode match_mode,
    SelectorProfile profile,
    const Referrer& referrer,
    bool is_html_document,
    bool use_legacy_background_size_shorthand_behavior,
    ContentSecurityPolicyDisposition policy_disposition,
    const Document* use_counter_document)
    : base_url_(base_url),
      charset_(charset),
      mode_(mode),
      match_mode_(match_mode),
      profile_(profile),
      referrer_(referrer),
      is_html_document_(is_html_document),
      use_legacy_background_size_shorthand_behavior_(
          use_legacy_background_size_shorthand_behavior),
      should_check_content_security_policy_(policy_disposition),
      document_(use_counter_document) {}

bool CSSParserContext::operator==(const CSSParserContext& other) const {
  return base_url_ == other.base_url_ && charset_ == other.charset_ &&
         mode_ == other.mode_ && match_mode_ == other.match_mode_ &&
         profile_ == other.profile_ &&
         is_html_document_ == other.is_html_document_ &&
         use_legacy_background_size_shorthand_behavior_ ==
             other.use_legacy_background_size_shorthand_behavior_;
}

const CSSParserContext* StrictCSSParserContext() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<CSSParserContext>>,
                                  strict_context_pool, ());
  Persistent<CSSParserContext>& context = *strict_context_pool;
  if (!context) {
    context = CSSParserContext::Create(kHTMLStandardMode);
    context.RegisterAsStaticReference();
  }

  return context;
}

KURL CSSParserContext::CompleteURL(const String& url) const {
  if (url.IsNull())
    return KURL();
  if (!Charset().IsValid())
    return KURL(BaseURL(), url);
  return KURL(BaseURL(), url, Charset());
}

void CSSParserContext::Count(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled())
    UseCounter::Count(*document_, feature);
}

void CSSParserContext::CountDeprecation(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled())
    Deprecation::CountDeprecation(*document_, feature);
}

void CSSParserContext::Count(CSSParserMode mode, CSSPropertyID property) const {
  if (IsUseCounterRecordingEnabled() && document_->GetPage()) {
    UseCounter* use_counter = &document_->GetPage()->GetUseCounter();
    if (use_counter)
      use_counter->Count(mode, property);
  }
}

bool CSSParserContext::IsDocumentHandleEqual(const Document* other) const {
  return document_.Get() == other;
}

void CSSParserContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
