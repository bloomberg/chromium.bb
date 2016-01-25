// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/HttpEquiv.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"

namespace blink {

void HttpEquiv::process(Document& document, const AtomicString& equiv, const AtomicString& content, bool inDocumentHeadElement)
{
    ASSERT(!equiv.isNull() && !content.isNull());

    if (equalIgnoringCase(equiv, "default-style")) {
        processHttpEquivDefaultStyle(document, content);
    } else if (equalIgnoringCase(equiv, "refresh")) {
        processHttpEquivRefresh(document, content);
    } else if (equalIgnoringCase(equiv, "set-cookie")) {
        processHttpEquivSetCookie(document, content);
    } else if (equalIgnoringCase(equiv, "content-language")) {
        document.setContentLanguage(content);
    } else if (equalIgnoringCase(equiv, "x-dns-prefetch-control")) {
        document.parseDNSPrefetchControlHeader(content);
    } else if (equalIgnoringCase(equiv, "x-frame-options")) {
        processHttpEquivXFrameOptions(document, content);
    } else if (equalIgnoringCase(equiv, "accept-ch")) {
        processHttpEquivAcceptCH(document, content);
    } else if (equalIgnoringCase(equiv, "content-security-policy") || equalIgnoringCase(equiv, "content-security-policy-report-only")) {
        if (inDocumentHeadElement)
            processHttpEquivContentSecurityPolicy(document, equiv, content);
        else
            document.contentSecurityPolicy()->reportMetaOutsideHead(content);
    }
}

void HttpEquiv::processHttpEquivContentSecurityPolicy(Document& document, const AtomicString& equiv, const AtomicString& content)
{
    if (document.importLoader())
        return;
    if (equalIgnoringCase(equiv, "content-security-policy"))
        document.contentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceMeta);
    else if (equalIgnoringCase(equiv, "content-security-policy-report-only"))
        document.contentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceMeta);
    else
        ASSERT_NOT_REACHED();
}

void HttpEquiv::processHttpEquivAcceptCH(Document& document, const AtomicString& content)
{
    if (!document.frame())
        return;

    UseCounter::count(document, UseCounter::ClientHintsMetaAcceptCH);
    document.clientHintsPreferences().updateFromAcceptClientHintsHeader(content, document.fetcher());
}

void HttpEquiv::processHttpEquivDefaultStyle(Document& document, const AtomicString& content)
{
    // The preferred style set has been overridden as per section
    // 14.3.2 of the HTML4.0 specification. We need to update the
    // sheet used variable and then update our style selector.
    // For more info, see the test at:
    // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
    // -dwh
    document.styleEngine().setSelectedStylesheetSetName(content);
    document.styleEngine().setPreferredStylesheetSetName(content);
    document.styleResolverChanged();
}

void HttpEquiv::processHttpEquivRefresh(Document& document, const AtomicString& content)
{
    document.maybeHandleHttpRefresh(content, Document::HttpRefreshFromMetaTag);
}

void HttpEquiv::processHttpEquivSetCookie(Document& document, const AtomicString& content)
{
    // FIXME: make setCookie work on XML documents too; e.g. in case of <html:meta .....>
    if (!document.isHTMLDocument())
        return;

    // Exception (for sandboxed documents) ignored.
    toHTMLDocument(document).setCookie(content, IGNORE_EXCEPTION);
}

void HttpEquiv::processHttpEquivXFrameOptions(Document& document, const AtomicString& content)
{
    LocalFrame* frame = document.frame();
    if (!frame)
        return;

    unsigned long requestIdentifier = document.loader()->mainResourceIdentifier();
    if (!frame->loader().shouldInterruptLoadForXFrameOptions(content, document.url(), requestIdentifier))
        return;

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel,
        "Refused to display '" + document.url().elidedString() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.");
    consoleMessage->setRequestIdentifier(requestIdentifier);
    document.addConsoleMessage(consoleMessage.release());

    frame->loader().stopAllLoaders();
    // Stopping the loader isn't enough, as we're already parsing the document; to honor the header's
    // intent, we must navigate away from the possibly partially-rendered document to a location that
    // doesn't inherit the parent's SecurityOrigin.
    // TODO(dglazkov): This should probably check document lifecycle instead.
    if (document.frame())
        frame->navigate(document, SecurityOrigin::urlWithUniqueSecurityOrigin(), true, UserGestureStatus::None);
}

} // namespace blink
