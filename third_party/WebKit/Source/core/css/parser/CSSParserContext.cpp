// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserContext.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/page/Page.h"

namespace blink {

// static
CSSParserContext* CSSParserContext::createWithStyleSheet(
    const CSSParserContext* other,
    const CSSStyleSheet* styleSheet) {
  return CSSParserContext::create(
      other, CSSStyleSheet::singleOwnerDocument(styleSheet));
}

// static
CSSParserContext* CSSParserContext::createWithStyleSheetContents(
    const CSSParserContext* other,
    const StyleSheetContents* styleSheetContents) {
  return CSSParserContext::create(
      other, StyleSheetContents::singleOwnerDocument(styleSheetContents));
}

// static
CSSParserContext* CSSParserContext::create(const CSSParserContext* other,
                                           const Document* m_document) {
  return new CSSParserContext(
      other->m_baseURL, other->m_charset, other->m_mode, other->m_matchMode,
      other->m_profile, other->m_referrer, other->m_isHTMLDocument,
      other->m_useLegacyBackgroundSizeShorthandBehavior,
      other->m_shouldCheckContentSecurityPolicy, m_document);
}

// static
CSSParserContext* CSSParserContext::create(CSSParserMode mode,
                                           SelectorProfile profile,
                                           const Document* m_document) {
  return new CSSParserContext(KURL(), emptyString, mode, mode, profile,
                              Referrer(), false, false,
                              DoNotCheckContentSecurityPolicy, m_document);
}

// static
CSSParserContext* CSSParserContext::create(const Document& document,
                                           const Document* m_document) {
  return CSSParserContext::create(document, KURL(), emptyString, DynamicProfile,
                                  m_document);
}

// static
CSSParserContext* CSSParserContext::create(const Document& document,
                                           const KURL& baseURLOverride,
                                           const String& charset,
                                           SelectorProfile profile,
                                           const Document* m_document) {
  const KURL baseURL =
      baseURLOverride.isNull() ? document.baseURL() : baseURLOverride;

  CSSParserMode mode =
      document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode;
  CSSParserMode matchMode;
  if (HTMLImportsController* importsController = document.importsController()) {
    matchMode = importsController->master()->inQuirksMode() ? HTMLQuirksMode
                                                            : HTMLStandardMode;
  } else {
    matchMode = mode;
  }

  const Referrer referrer(baseURL.strippedForUseAsReferrer(),
                          document.getReferrerPolicy());

  bool useLegacyBackgroundSizeShorthandBehavior =
      document.settings()
          ? document.settings()->getUseLegacyBackgroundSizeShorthandBehavior()
          : false;

  ContentSecurityPolicyDisposition policyDisposition;
  if (ContentSecurityPolicy::shouldBypassMainWorld(&document))
    policyDisposition = DoNotCheckContentSecurityPolicy;
  else
    policyDisposition = CheckContentSecurityPolicy;

  return new CSSParserContext(baseURL, charset, mode, matchMode, profile,
                              referrer, document.isHTMLDocument(),
                              useLegacyBackgroundSizeShorthandBehavior,
                              policyDisposition, m_document);
}

CSSParserContext::CSSParserContext(
    const KURL& baseURL,
    const String& charset,
    CSSParserMode mode,
    CSSParserMode matchMode,
    SelectorProfile profile,
    const Referrer& referrer,
    bool isHTMLDocument,
    bool useLegacyBackgroundSizeShorthandBehavior,
    ContentSecurityPolicyDisposition policyDisposition,
    const Document* m_document)
    : m_baseURL(baseURL),
      m_charset(charset),
      m_mode(mode),
      m_matchMode(matchMode),
      m_profile(profile),
      m_referrer(referrer),
      m_isHTMLDocument(isHTMLDocument),
      m_useLegacyBackgroundSizeShorthandBehavior(
          useLegacyBackgroundSizeShorthandBehavior),
      m_shouldCheckContentSecurityPolicy(policyDisposition),
      m_document(m_document) {}

bool CSSParserContext::operator==(const CSSParserContext& other) const {
  return m_baseURL == other.m_baseURL && m_charset == other.m_charset &&
         m_mode == other.m_mode && m_matchMode == other.m_matchMode &&
         m_profile == other.m_profile &&
         m_isHTMLDocument == other.m_isHTMLDocument &&
         m_useLegacyBackgroundSizeShorthandBehavior ==
             other.m_useLegacyBackgroundSizeShorthandBehavior;
}

const CSSParserContext* strictCSSParserContext() {
  DEFINE_STATIC_LOCAL(CSSParserContext, strictContext,
                      (CSSParserContext::create(HTMLStandardMode)));
  return &strictContext;
}

KURL CSSParserContext::completeURL(const String& url) const {
  if (url.isNull())
    return KURL();
  if (charset().isEmpty())
    return KURL(baseURL(), url);
  return KURL(baseURL(), url, charset());
}

void CSSParserContext::count(UseCounter::Feature feature) const {
  if (isUseCounterRecordingEnabled())
    UseCounter::count(*m_document, feature);
}

void CSSParserContext::countDeprecation(UseCounter::Feature feature) const {
  if (isUseCounterRecordingEnabled())
    Deprecation::countDeprecation(*m_document, feature);
}

void CSSParserContext::count(CSSParserMode mode, CSSPropertyID property) const {
  if (isUseCounterRecordingEnabled() && m_document->page()) {
    UseCounter* useCounter = &m_document->page()->useCounter();
    if (useCounter)
      useCounter->count(mode, property);
  }
}

bool CSSParserContext::isDocumentHandleEqual(const Document* other) const {
  return m_document.get() == other;
}

DEFINE_TRACE(CSSParserContext) {
  visitor->trace(m_document);
}

}  // namespace blink
