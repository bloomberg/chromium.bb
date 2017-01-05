// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserContext.h"

#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/imports/HTMLImportsController.h"

namespace blink {

CSSParserContext::CSSParserContext(CSSParserMode mode,
                                   UseCounter* useCounter,
                                   SelectorProfile profile)
    : m_mode(mode),
      m_matchMode(mode),
      m_profile(profile),
      m_isHTMLDocument(false),
      m_useLegacyBackgroundSizeShorthandBehavior(false),
      m_shouldCheckContentSecurityPolicy(DoNotCheckContentSecurityPolicy),
      m_useCounter(useCounter) {}

CSSParserContext::CSSParserContext(const Document& document,
                                   UseCounter* useCounter,
                                   const KURL& baseURL,
                                   const String& charset,
                                   SelectorProfile profile)
    : m_baseURL(baseURL.isNull() ? document.baseURL() : baseURL),
      m_charset(charset),
      m_mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode),
      m_profile(profile),
      m_referrer(m_baseURL.strippedForUseAsReferrer(),
                 document.getReferrerPolicy()),
      m_isHTMLDocument(document.isHTMLDocument()),
      m_useLegacyBackgroundSizeShorthandBehavior(
          document.settings()
              ? document.settings()
                    ->getUseLegacyBackgroundSizeShorthandBehavior()
              : false),
      m_shouldCheckContentSecurityPolicy(DoNotCheckContentSecurityPolicy),
      m_useCounter(useCounter) {
  if (ContentSecurityPolicy::shouldBypassMainWorld(&document))
    m_shouldCheckContentSecurityPolicy = DoNotCheckContentSecurityPolicy;
  else
    m_shouldCheckContentSecurityPolicy = CheckContentSecurityPolicy;

  if (HTMLImportsController* importsController = document.importsController()) {
    m_matchMode = importsController->master()->inQuirksMode()
                      ? HTMLQuirksMode
                      : HTMLStandardMode;
  } else {
    m_matchMode = m_mode;
  }
}

CSSParserContext::CSSParserContext(const CSSParserContext& other,
                                   UseCounter* useCounter)
    : m_baseURL(other.m_baseURL),
      m_charset(other.m_charset),
      m_mode(other.m_mode),
      m_matchMode(other.m_matchMode),
      m_profile(other.m_profile),
      m_referrer(other.m_referrer),
      m_isHTMLDocument(other.m_isHTMLDocument),
      m_useLegacyBackgroundSizeShorthandBehavior(
          other.m_useLegacyBackgroundSizeShorthandBehavior),
      m_shouldCheckContentSecurityPolicy(
          other.m_shouldCheckContentSecurityPolicy),
      m_useCounter(useCounter) {}

bool CSSParserContext::operator==(const CSSParserContext& other) const {
  return m_baseURL == other.m_baseURL && m_charset == other.m_charset &&
         m_mode == other.m_mode && m_matchMode == other.m_matchMode &&
         m_profile == other.m_profile &&
         m_isHTMLDocument == other.m_isHTMLDocument &&
         m_useLegacyBackgroundSizeShorthandBehavior ==
             other.m_useLegacyBackgroundSizeShorthandBehavior;
}

const CSSParserContext& strictCSSParserContext() {
  DEFINE_STATIC_LOCAL(CSSParserContext, strictContext,
                      (HTMLStandardMode, nullptr));
  return strictContext;
}

KURL CSSParserContext::completeURL(const String& url) const {
  if (url.isNull())
    return KURL();
  if (charset().isEmpty())
    return KURL(baseURL(), url);
  return KURL(baseURL(), url, charset());
}

}  // namespace blink
