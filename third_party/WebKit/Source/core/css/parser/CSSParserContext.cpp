// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserContext.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/imports/HTMLImportsController.h"

namespace blink {

// static
CSSParserContext* CSSParserContext::createWithStyleSheet(
    const CSSParserContext* other,
    const CSSStyleSheet* styleSheet) {
  return CSSParserContext::create(other, UseCounter::getFrom(styleSheet));
}

// static
CSSParserContext* CSSParserContext::createWithStyleSheetContents(
    const CSSParserContext* other,
    const StyleSheetContents* styleSheetContents) {
  return CSSParserContext::create(other,
                                  UseCounter::getFrom(styleSheetContents));
}

// static
CSSParserContext* CSSParserContext::create(const CSSParserContext* other,
                                           UseCounter* useCounter) {
  return new CSSParserContext(
      other->m_baseURL, other->m_charset, other->m_mode, other->m_matchMode,
      other->m_profile, other->m_referrer, other->m_isHTMLDocument,
      other->m_useLegacyBackgroundSizeShorthandBehavior,
      other->m_shouldCheckContentSecurityPolicy, useCounter);
}

// static
CSSParserContext* CSSParserContext::create(CSSParserMode mode,
                                           SelectorProfile profile,
                                           UseCounter* useCounter) {
  return new CSSParserContext(KURL(), emptyString(), mode, mode, profile,
                              Referrer(), false, false,
                              DoNotCheckContentSecurityPolicy, useCounter);
}

// static
CSSParserContext* CSSParserContext::create(const Document& document,
                                           UseCounter* useCounter) {
  return CSSParserContext::create(document, KURL(), emptyString(),
                                  DynamicProfile, useCounter);
}

// static
CSSParserContext* CSSParserContext::create(const Document& document,
                                           const KURL& baseURLOverride,
                                           const String& charset,
                                           SelectorProfile profile,
                                           UseCounter* useCounter) {
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
                              policyDisposition, useCounter);
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
    UseCounter* useCounter)
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
      m_useCounter(useCounter) {}

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

}  // namespace blink
