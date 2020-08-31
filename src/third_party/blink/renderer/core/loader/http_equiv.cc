// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/http_equiv.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/private/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"

namespace blink {

namespace {

// Returns true if execution of scripts from the url are allowed. Compared to
// AllowScriptFromSource(), this method does not generate any
// notification to the |ContentSettingsClient| that the execution of the
// script was blocked. This method should be called only when there is a need
// to check the settings, and where blocked setting doesn't really imply that
// JavaScript was blocked from being executed.
bool AllowScriptFromSourceWithoutNotifying(
    const KURL& url,
    WebContentSettingsClient* settings_client,
    Settings* settings) {
  bool allow_script = !settings || settings->GetScriptEnabled();
  if (settings_client)
    allow_script = settings_client->AllowScriptFromSource(allow_script, url);
  return allow_script;
}

}  // namespace

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
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "X-Frame-Options may only be set via an HTTP header sent along with a "
        "document. It may not be set inside <meta>."));
  } else if (EqualIgnoringASCIICase(equiv, http_names::kAcceptCH)) {
    ProcessHttpEquivAcceptCH(document, content);
  } else if (EqualIgnoringASCIICase(equiv, "content-security-policy") ||
             EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    if (in_document_head_element)
      ProcessHttpEquivContentSecurityPolicy(document, equiv, content);
    else
      document.GetContentSecurityPolicy()->ReportMetaOutsideHead(content);
  } else if (EqualIgnoringASCIICase(equiv, http_names::kOriginTrial)) {
    if (in_document_head_element)
      document.GetOriginTrialContext()->AddToken(content);
  }
}

void HttpEquiv::ProcessHttpEquivContentSecurityPolicy(
    Document& document,
    const AtomicString& equiv,
    const AtomicString& content) {
  if (document.ImportLoader())
    return;
  if (document.GetSettings() && document.GetSettings()->BypassCSP())
    return;
  if (EqualIgnoringASCIICase(equiv, "content-security-policy")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, network::mojom::ContentSecurityPolicyType::kEnforce,
        network::mojom::ContentSecurityPolicySource::kMeta);
  } else if (EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, network::mojom::ContentSecurityPolicyType::kReport,
        network::mojom::ContentSecurityPolicySource::kMeta);
  } else {
    NOTREACHED();
  }
}

void HttpEquiv::ProcessHttpEquivAcceptCH(Document& document,
                                         const AtomicString& content) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  if (!document.GetFrame()->IsMainFrame()) {
    return;
  }

  if (!AllowScriptFromSourceWithoutNotifying(
          document.Url(), document.GetFrame()->GetContentSettingsClient(),
          document.GetFrame()->GetSettings())) {
    // Do not allow configuring client hints if JavaScript is disabled.
    return;
  }

  UseCounter::Count(document, WebFeature::kClientHintsMetaAcceptCH);
  FrameClientHintsPreferencesContext hints_context(frame);
  frame->GetClientHintsPreferences().UpdateFromAcceptClientHintsHeader(
      content, document.Url(), ClientHintsPreferences::UpdateMode::kMerge,
      &hints_context);
}

void HttpEquiv::ProcessHttpEquivDefaultStyle(Document& document,
                                             const AtomicString& content) {
  document.GetStyleEngine().SetHttpDefaultStyle(content);
}

void HttpEquiv::ProcessHttpEquivRefresh(Document& document,
                                        const AtomicString& content,
                                        Element* element) {
  UseCounter::Count(document, WebFeature::kMetaRefresh);
  if (!document.GetContentSecurityPolicy()->AllowInline(
          ContentSecurityPolicy::InlineType::kScript, element, "" /* content */,
          "" /* nonce */, NullURL(), OrdinalNumber(),
          ReportingDisposition::kSuppressReporting)) {
    UseCounter::Count(document,
                      WebFeature::kMetaRefreshWhenCSPBlocksInlineScript);
  }

  document.MaybeHandleHttpRefresh(content, Document::kHttpRefreshFromMetaTag);
}

void HttpEquiv::ProcessHttpEquivSetCookie(Document& document,
                                          const AtomicString& content,
                                          Element* element) {
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError,
      String::Format("Blocked setting the `%s` cookie from a `<meta>` tag.",
                     content.Utf8().c_str())));
}

}  // namespace blink
