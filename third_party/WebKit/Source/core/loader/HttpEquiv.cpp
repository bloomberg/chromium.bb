// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/HttpEquiv.h"

#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/private/FrameClientHintsPreferencesContext.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/HTTPNames.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"

namespace blink {

void HttpEquiv::process(Document& document,
                        const AtomicString& equiv,
                        const AtomicString& content,
                        bool inDocumentHeadElement,
                        Element* element) {
  DCHECK(!equiv.isNull());
  DCHECK(!content.isNull());

  if (equalIgnoringASCIICase(equiv, "default-style")) {
    processHttpEquivDefaultStyle(document, content);
  } else if (equalIgnoringASCIICase(equiv, "refresh")) {
    processHttpEquivRefresh(document, content, element);
  } else if (equalIgnoringASCIICase(equiv, "set-cookie")) {
    processHttpEquivSetCookie(document, content, element);
  } else if (equalIgnoringASCIICase(equiv, "content-language")) {
    document.setContentLanguage(content);
  } else if (equalIgnoringASCIICase(equiv, "x-dns-prefetch-control")) {
    document.parseDNSPrefetchControlHeader(content);
  } else if (equalIgnoringASCIICase(equiv, "x-frame-options")) {
    document.addConsoleMessage(ConsoleMessage::create(
        SecurityMessageSource, ErrorMessageLevel,
        "X-Frame-Options may only be set via an HTTP header sent along with a "
        "document. It may not be set inside <meta>."));
  } else if (equalIgnoringASCIICase(equiv, "accept-ch")) {
    processHttpEquivAcceptCH(document, content);
  } else if (equalIgnoringASCIICase(equiv, "content-security-policy") ||
             equalIgnoringASCIICase(equiv, "content-security-policy-report-only")) {
    if (inDocumentHeadElement)
      processHttpEquivContentSecurityPolicy(document, equiv, content);
    else
      document.contentSecurityPolicy()->reportMetaOutsideHead(content);
  } else if (equalIgnoringASCIICase(equiv, "suborigin")) {
    document.addConsoleMessage(ConsoleMessage::create(
        SecurityMessageSource, ErrorMessageLevel,
        "Error with Suborigin header: Suborigin header with value '" + content +
            "' was delivered via a <meta> element and not an HTTP header, "
            "which is disallowed. The Suborigin has been ignored."));
  } else if (equalIgnoringASCIICase(equiv, HTTPNames::Origin_Trial)) {
    if (inDocumentHeadElement)
      OriginTrialContext::from(&document)->addToken(content);
  }
}

void HttpEquiv::processHttpEquivContentSecurityPolicy(
    Document& document,
    const AtomicString& equiv,
    const AtomicString& content) {
  if (document.importLoader())
    return;
  if (equalIgnoringASCIICase(equiv, "content-security-policy")) {
    document.contentSecurityPolicy()->didReceiveHeader(
        content, ContentSecurityPolicyHeaderTypeEnforce,
        ContentSecurityPolicyHeaderSourceMeta);
  } else if (equalIgnoringASCIICase(equiv, "content-security-policy-report-only")) {
    document.contentSecurityPolicy()->didReceiveHeader(
        content, ContentSecurityPolicyHeaderTypeReport,
        ContentSecurityPolicyHeaderSourceMeta);
  } else {
    NOTREACHED();
  }
}

void HttpEquiv::processHttpEquivAcceptCH(Document& document,
                                         const AtomicString& content) {
  if (!document.frame())
    return;

  UseCounter::count(document, UseCounter::ClientHintsMetaAcceptCH);
  FrameClientHintsPreferencesContext hintsContext(document.frame());
  document.clientHintsPreferences().updateFromAcceptClientHintsHeader(
      content, &hintsContext);
}

void HttpEquiv::processHttpEquivDefaultStyle(Document& document,
                                             const AtomicString& content) {
  document.styleEngine().setHttpDefaultStyle(content);
}

void HttpEquiv::processHttpEquivRefresh(Document& document,
                                        const AtomicString& content,
                                        Element* element) {
  UseCounter::count(document, UseCounter::MetaRefresh);
  if (!document.contentSecurityPolicy()->allowInlineScript(
          element, KURL(), "", OrdinalNumber(), "",
          SecurityViolationReportingPolicy::SuppressReporting)) {
    UseCounter::count(document,
                      UseCounter::MetaRefreshWhenCSPBlocksInlineScript);
  }

  document.maybeHandleHttpRefresh(content, Document::HttpRefreshFromMetaTag);
}

void HttpEquiv::processHttpEquivSetCookie(Document& document,
                                          const AtomicString& content,
                                          Element* element) {
  // FIXME: make setCookie work on XML documents too; e.g. in case of
  // <html:meta.....>
  if (!document.isHTMLDocument())
    return;

  UseCounter::count(document, UseCounter::MetaSetCookie);
  if (!document.contentSecurityPolicy()->allowInlineScript(
          element, KURL(), "", OrdinalNumber(), "",
          SecurityViolationReportingPolicy::SuppressReporting)) {
    UseCounter::count(document,
                      UseCounter::MetaSetCookieWhenCSPBlocksInlineScript);
  }

  // Exception (for sandboxed documents) ignored.
  document.setCookie(content, IGNORE_EXCEPTION_FOR_TESTING);
}

}  // namespace blink
