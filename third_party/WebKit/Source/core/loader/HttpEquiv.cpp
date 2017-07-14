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

void HttpEquiv::Process(Document& document,
                        const AtomicString& equiv,
                        const AtomicString& content,
                        bool in_document_head_element,
                        Element* element) {
  DCHECK(!equiv.IsNull());
  DCHECK(!content.IsNull());

  if (EqualIgnoringASCIICase(equiv, "default-style")) {
    ProcessHttpEquivDefaultStyle(document, content);
  } else if (EqualIgnoringASCIICase(equiv, "refresh")) {
    ProcessHttpEquivRefresh(document, content, element);
  } else if (EqualIgnoringASCIICase(equiv, "set-cookie")) {
    ProcessHttpEquivSetCookie(document, content, element);
  } else if (EqualIgnoringASCIICase(equiv, "content-language")) {
    document.SetContentLanguage(content);
  } else if (EqualIgnoringASCIICase(equiv, "x-dns-prefetch-control")) {
    document.ParseDNSPrefetchControlHeader(content);
  } else if (EqualIgnoringASCIICase(equiv, "x-frame-options")) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "X-Frame-Options may only be set via an HTTP header sent along with a "
        "document. It may not be set inside <meta>."));
  } else if (EqualIgnoringASCIICase(equiv, "accept-ch")) {
    ProcessHttpEquivAcceptCH(document, content);
  } else if (EqualIgnoringASCIICase(equiv, "content-security-policy") ||
             EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    if (in_document_head_element)
      ProcessHttpEquivContentSecurityPolicy(document, equiv, content);
    else
      document.GetContentSecurityPolicy()->ReportMetaOutsideHead(content);
  } else if (EqualIgnoringASCIICase(equiv, "suborigin")) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Error with Suborigin header: Suborigin header with value '" + content +
            "' was delivered via a <meta> element and not an HTTP header, "
            "which is disallowed. The Suborigin has been ignored."));
  } else if (EqualIgnoringASCIICase(equiv, HTTPNames::Origin_Trial)) {
    if (in_document_head_element)
      OriginTrialContext::From(&document)->AddToken(content);
  }
}

void HttpEquiv::ProcessHttpEquivContentSecurityPolicy(
    Document& document,
    const AtomicString& equiv,
    const AtomicString& content) {
  if (document.ImportLoader())
    return;
  if (EqualIgnoringASCIICase(equiv, "content-security-policy")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, kContentSecurityPolicyHeaderTypeEnforce,
        kContentSecurityPolicyHeaderSourceMeta);
  } else if (EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, kContentSecurityPolicyHeaderTypeReport,
        kContentSecurityPolicyHeaderSourceMeta);
  } else {
    NOTREACHED();
  }
}

void HttpEquiv::ProcessHttpEquivAcceptCH(Document& document,
                                         const AtomicString& content) {
  if (!document.GetFrame())
    return;

  UseCounter::Count(document, WebFeature::kClientHintsMetaAcceptCH);
  FrameClientHintsPreferencesContext hints_context(document.GetFrame());
  document.GetClientHintsPreferences().UpdateFromAcceptClientHintsHeader(
      content, &hints_context);
}

void HttpEquiv::ProcessHttpEquivDefaultStyle(Document& document,
                                             const AtomicString& content) {
  document.GetStyleEngine().SetHttpDefaultStyle(content);
}

void HttpEquiv::ProcessHttpEquivRefresh(Document& document,
                                        const AtomicString& content,
                                        Element* element) {
  UseCounter::Count(document, WebFeature::kMetaRefresh);
  if (!document.GetContentSecurityPolicy()->AllowInlineScript(
          element, NullURL(), "", OrdinalNumber(), "",
          ContentSecurityPolicy::InlineType::kBlock,
          SecurityViolationReportingPolicy::kSuppressReporting)) {
    UseCounter::Count(document,
                      WebFeature::kMetaRefreshWhenCSPBlocksInlineScript);
  }

  document.MaybeHandleHttpRefresh(content, Document::kHttpRefreshFromMetaTag);
}

void HttpEquiv::ProcessHttpEquivSetCookie(Document& document,
                                          const AtomicString& content,
                                          Element* element) {
  // FIXME: make setCookie work on XML documents too; e.g. in case of
  // <html:meta.....>
  if (!document.IsHTMLDocument())
    return;

  UseCounter::Count(document, WebFeature::kMetaSetCookie);
  if (!document.GetContentSecurityPolicy()->AllowInlineScript(
          element, NullURL(), "", OrdinalNumber(), "",
          ContentSecurityPolicy::InlineType::kBlock,
          SecurityViolationReportingPolicy::kSuppressReporting)) {
    UseCounter::Count(document,
                      WebFeature::kMetaSetCookieWhenCSPBlocksInlineScript);
  }

  // Exception (for sandboxed documents) ignored.
  document.setCookie(content, IGNORE_EXCEPTION_FOR_TESTING);
}

}  // namespace blink
