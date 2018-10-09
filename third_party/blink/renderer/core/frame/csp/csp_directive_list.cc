// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

#include <memory>
#include <utility>
#include <vector>

#include "services/network/public/cpp/features.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String GetSha256String(const String& content) {
  DigestValue digest;
  StringUTF8Adaptor utf8_content(content);
  bool digest_success = ComputeDigest(kHashAlgorithmSha256, utf8_content.Data(),
                                      utf8_content.length(), digest);
  if (!digest_success) {
    return "sha256-...";
  }

  return "sha256-" + Base64Encode(reinterpret_cast<char*>(digest.data()),
                                  digest.size(), kBase64DoNotInsertLFs);
}

ContentSecurityPolicyHashAlgorithm ConvertHashAlgorithmToCSPHashAlgorithm(
    IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return kContentSecurityPolicyHashAlgorithmSha256;
    case IntegrityAlgorithm::kSha384:
      return kContentSecurityPolicyHashAlgorithmSha384;
    case IntegrityAlgorithm::kSha512:
      return kContentSecurityPolicyHashAlgorithmSha512;
    case IntegrityAlgorithm::kEd25519:
      return kContentSecurityPolicyHashAlgorithmEd25519;
  }
  NOTREACHED();
  return kContentSecurityPolicyHashAlgorithmNone;
}

// IntegrityMetadata (from SRI) has base64-encoded digest values, but CSP uses
// binary format. This converts from the former to the latter.
bool ParseBase64Digest(String base64, DigestValue* hash) {
  Vector<char> hash_vector;
  // We accept base64url-encoded data here by normalizing it to base64.
  if (!Base64Decode(NormalizeToBase64(base64), hash_vector))
    return false;
  if (hash_vector.IsEmpty() || hash_vector.size() > kMaxDigestSize)
    return false;
  hash->Append(reinterpret_cast<uint8_t*>(hash_vector.data()),
               hash_vector.size());
  return true;
}

}  // namespace

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy,
                                   ContentSecurityPolicyHeaderType type,
                                   ContentSecurityPolicyHeaderSource source)
    : policy_(policy),
      header_type_(type),
      header_source_(source),
      has_sandbox_policy_(false),
      strict_mixed_content_checking_enforced_(false),
      upgrade_insecure_requests_(false),
      treat_as_public_address_(false),
      require_sri_for_(RequireSRIForToken::kNone),
      use_reporting_api_(false) {}

CSPDirectiveList* CSPDirectiveList::Create(
    ContentSecurityPolicy* policy,
    const UChar* begin,
    const UChar* end,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source,
    bool should_parse_wasm_eval) {
  CSPDirectiveList* directives = new CSPDirectiveList(policy, type, source);
  directives->Parse(begin, end, should_parse_wasm_eval);

  if (!directives->CheckEval(directives->OperativeDirective(
          ContentSecurityPolicy::DirectiveType::kScriptSrc))) {
    String message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        directives
            ->OperativeDirective(
                ContentSecurityPolicy::DirectiveType::kScriptSrc)
            ->GetText() +
        "\".\n";
    directives->SetEvalDisabledErrorMessage(message);
  }

  if (directives->IsReportOnly() &&
      source != kContentSecurityPolicyHeaderSourceMeta &&
      directives->ReportEndpoints().IsEmpty()) {
    policy->ReportMissingReportURI(
        String(begin, static_cast<wtf_size_t>(end - begin)));
  }

  return directives;
}

void CSPDirectiveList::ReportViolation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    ResourceRequest::RedirectStatus redirect_status) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  policy_->LogToConsole(ConsoleMessage::Create(kSecurityMessageSource,
                                               kErrorMessageLevel, message));
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, ContentSecurityPolicy::kURLViolation,
                           std::unique_ptr<SourceLocation>(),
                           nullptr,  // localFrame
                           redirect_status);
}

void CSPDirectiveList::ReportViolationWithFrame(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    LocalFrame* frame) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  policy_->LogToConsole(ConsoleMessage::Create(kSecurityMessageSource,
                                               kErrorMessageLevel, message),
                        frame);
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, ContentSecurityPolicy::kURLViolation,
                           std::unique_ptr<SourceLocation>(), frame);
}

void CSPDirectiveList::ReportViolationWithLocation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& console_message,
    const KURL& blocked_url,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    Element* element,
    const String& source) const {
  String message =
      IsReportOnly() ? "[Report Only] " + console_message : console_message;
  std::unique_ptr<SourceLocation> source_location =
      SourceLocation::Capture(context_url, context_line.OneBasedInt(), 0);
  policy_->LogToConsole(ConsoleMessage::Create(kSecurityMessageSource,
                                               kErrorMessageLevel, message,
                                               source_location->Clone()));
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_,
                           ContentSecurityPolicy::kInlineViolation,
                           std::move(source_location), nullptr,  // localFrame
                           RedirectStatus::kNoRedirect, element, source);
}

void CSPDirectiveList::ReportEvalViolation(
    const String& directive_text,
    const ContentSecurityPolicy::DirectiveType effective_type,
    const String& message,
    const KURL& blocked_url,
    ScriptState* script_state,
    const ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  String report_message = IsReportOnly() ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a
  // JavaScript exception that the caller will throw. (Exceptions will
  // never get thrown in report-only mode because the caller won't see
  // a violation.)
  if (IsReportOnly() ||
      exception_status == ContentSecurityPolicy::kWillNotThrowException) {
    ConsoleMessage* console_message = ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel, report_message);
    policy_->LogToConsole(console_message);
  }
  policy_->ReportViolation(directive_text, effective_type, message, blocked_url,
                           report_endpoints_, use_reporting_api_, header_,
                           header_type_, ContentSecurityPolicy::kEvalViolation,
                           std::unique_ptr<SourceLocation>(), nullptr,
                           RedirectStatus::kFollowedRedirect, nullptr, content);
}

bool CSPDirectiveList::CheckEval(SourceListDirective* directive) const {
  return !directive || directive->AllowEval();
}

bool CSPDirectiveList::CheckWasmEval(SourceListDirective* directive) const {
  return !directive || directive->AllowWasmEval();
}

bool CSPDirectiveList::IsMatchingNoncePresent(SourceListDirective* directive,
                                              const String& nonce) const {
  return directive && directive->AllowNonce(nonce);
}

bool CSPDirectiveList::AreAllMatchingHashesPresent(
    SourceListDirective* directive,
    const IntegrityMetadataSet& hashes) const {
  if (!directive || hashes.IsEmpty())
    return false;
  for (const std::pair<String, IntegrityAlgorithm>& hash : hashes) {
    // Convert the hash from integrity metadata format to CSP format.
    CSPHashValue csp_hash;
    csp_hash.first = ConvertHashAlgorithmToCSPHashAlgorithm(hash.second);
    if (!ParseBase64Digest(hash.first, &csp_hash.second))
      return false;
    // All integrity hashes must be listed in the CSP.
    if (!directive->AllowHash(csp_hash))
      return false;
  }
  return true;
}

bool CSPDirectiveList::CheckHash(SourceListDirective* directive,
                                 const CSPHashValue& hash_value) const {
  return !directive || directive->AllowHash(hash_value);
}

bool CSPDirectiveList::CheckUnsafeHashesAllowed(
    SourceListDirective* directive) const {
  return !directive || directive->AllowUnsafeHashes();
}

bool CSPDirectiveList::CheckDynamic(SourceListDirective* directive) const {
  return !directive || directive->AllowDynamic();
}

void CSPDirectiveList::ReportMixedContent(
    const KURL& mixed_url,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (StrictMixedContentChecking()) {
    policy_->ReportViolation(
        ContentSecurityPolicy::GetDirectiveName(
            ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent),
        ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent, String(),
        mixed_url, report_endpoints_, use_reporting_api_, header_, header_type_,
        ContentSecurityPolicy::kURLViolation, std::unique_ptr<SourceLocation>(),
        nullptr,  // contextFrame,
        redirect_status);
  }
}

bool CSPDirectiveList::CheckSource(
    SourceListDirective* directive,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  return !directive || directive->Allows(url.IsEmpty() ? policy_->Url() : url,
                                         redirect_status);
}

bool CSPDirectiveList::CheckAncestors(SourceListDirective* directive,
                                      LocalFrame* frame) const {
  if (!frame || !directive)
    return true;

  for (Frame* current = frame->Tree().Parent(); current;
       current = current->Tree().Parent()) {
    // The |current| frame might be a remote frame which has no URL, so use
    // its origin instead.  This should suffice for this check since it
    // doesn't do path comparisons.  See https://crbug.com/582544.
    //
    // TODO(mkwst): Move this check up into the browser process.  See
    // https://crbug.com/555418.
    KURL url(NullURL(),
             current->GetSecurityContext()->GetSecurityOrigin()->ToString());
    if (!directive->Allows(url, ResourceRequest::RedirectStatus::kNoRedirect))
      return false;
  }
  return true;
}

bool CSPDirectiveList::CheckRequestWithoutIntegrity(
    mojom::RequestContextType context) const {
  if (require_sri_for_ == RequireSRIForToken::kNone)
    return true;
  // SRI specification
  // (https://w3c.github.io/webappsec-subresource-integrity/#apply-algorithm-to-request)
  // says to match token with request's destination with the token.
  // Keep this logic aligned with ContentSecurityPolicy::allowRequest
  if ((require_sri_for_ & RequireSRIForToken::kScript) &&
      (context == mojom::RequestContextType::SCRIPT ||
       context == mojom::RequestContextType::IMPORT ||
       context == mojom::RequestContextType::SERVICE_WORKER ||
       context == mojom::RequestContextType::SHARED_WORKER ||
       context == mojom::RequestContextType::WORKER)) {
    return false;
  }
  if ((require_sri_for_ & RequireSRIForToken::kStyle) &&
      context == mojom::RequestContextType::STYLE)
    return false;
  return true;
}

bool CSPDirectiveList::CheckRequestWithoutIntegrityAndReportViolation(
    mojom::RequestContextType context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (CheckRequestWithoutIntegrity(context))
    return true;
  String resource_type;
  switch (context) {
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::IMPORT:
      resource_type = "script";
      break;
    case mojom::RequestContextType::STYLE:
      resource_type = "stylesheet";
      break;
    case mojom::RequestContextType::SERVICE_WORKER:
      resource_type = "service worker";
      break;
    case mojom::RequestContextType::SHARED_WORKER:
      resource_type = "shared worker";
      break;
    case mojom::RequestContextType::WORKER:
      resource_type = "worker";
      break;
    default:
      break;
  }

  ReportViolation(ContentSecurityPolicy::GetDirectiveName(
                      ContentSecurityPolicy::DirectiveType::kRequireSRIFor),
                  ContentSecurityPolicy::DirectiveType::kRequireSRIFor,
                  "Refused to load the " + resource_type + " '" +
                      url.ElidedString() +
                      "' because 'require-sri-for' directive requires "
                      "integrity attribute be present for all " +
                      resource_type + "s.",
                  url, redirect_status);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowRequestWithoutIntegrity(
    mojom::RequestContextType context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (reporting_policy == SecurityViolationReportingPolicy::kReport)
    return CheckRequestWithoutIntegrityAndReportViolation(context, url,
                                                          redirect_status);
  return DenyIfEnforcingPolicy() || CheckRequestWithoutIntegrity(context);
}

bool CSPDirectiveList::CheckMediaType(MediaListDirective* directive,
                                      const String& type,
                                      const String& type_attribute) const {
  if (!directive)
    return true;
  if (type_attribute.IsEmpty() || type_attribute.StripWhiteSpace() != type)
    return false;
  return directive->Allows(type);
}

bool CSPDirectiveList::CheckEvalAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    ScriptState* script_state,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (CheckEval(directive))
    return true;

  String suffix = String();
  if (directive == default_src_)
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";

  ReportEvalViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kScriptSrc,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), script_state, exception_status,
      directive->AllowReportSample() ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckWasmEvalAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    ScriptState* script_state,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (CheckWasmEval(directive))
    return true;

  String suffix = String();
  if (directive == default_src_) {
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";
  }

  ReportEvalViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kScriptSrc,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), script_state, exception_status,
      directive->AllowReportSample() ? content : g_empty_string);
  if (!IsReportOnly()) {
    policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckMediaTypeAndReportViolation(
    MediaListDirective* directive,
    const String& type,
    const String& type_attribute,
    const String& console_message) const {
  if (CheckMediaType(directive, type, type_attribute))
    return true;

  String message = console_message + "\'" + directive->GetText() + "\'.";
  if (type_attribute.IsEmpty())
    message = message +
              " When enforcing the 'plugin-types' directive, the plugin's "
              "media type must be explicitly declared with a 'type' attribute "
              "on the containing element (e.g. '<object type=\"[TYPE GOES "
              "HERE]\" ...>').";

  // 'RedirectStatus::NoRedirect' is safe here, as we do the media type check
  // before actually loading data; this means that we shouldn't leak redirect
  // targets, as we won't have had a chance to redirect yet.
  ReportViolation(
      directive->GetText(), ContentSecurityPolicy::DirectiveType::kPluginTypes,
      message + "\n", NullURL(), ResourceRequest::RedirectStatus::kNoRedirect);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::CheckInlineAndReportViolation(
    SourceListDirective* directive,
    const String& console_message,
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    bool is_script,
    const String& hash_value,
    ContentSecurityPolicy::DirectiveType effective_type) const {
  if (!directive || directive->AllowAllInline())
    return true;

  String suffix = String();
  if (directive->AllowInline() && directive->IsHashOrNoncePresent()) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash ('" + hash_value +
        "'), or a nonce ('nonce-...') is required to enable inline execution.";
    if (directive == default_src_)
      suffix = suffix + " Note also that '" +
               String(is_script ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  ReportViolationWithLocation(
      directive->GetText(), effective_type,
      console_message + "\"" + directive->GetText() + "\"." + suffix + "\n",
      KURL(), context_url, context_line, element,
      directive->AllowReportSample() ? source : g_empty_string);

  if (!IsReportOnly()) {
    if (is_script)
      policy_->ReportBlockedScriptExecutionToInspector(directive->GetText());
    return false;
  }
  return true;
}

bool CSPDirectiveList::CheckSourceAndReportViolation(
    SourceListDirective* directive,
    const KURL& url,
    const ContentSecurityPolicy::DirectiveType effective_type,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (!directive)
    return true;

  // We ignore URL-based whitelists if we're allowing dynamic script injection.
  if (CheckSource(directive, url, redirect_status) && !CheckDynamic(directive))
    return true;

  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::kChildSrc, effective_type);
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::kDefaultSrc, effective_type);

  String prefix = "Refused to ";
  if (ContentSecurityPolicy::DirectiveType::kBaseURI == effective_type)
    prefix = prefix + "set the document's base URI to '";
  else if (ContentSecurityPolicy::DirectiveType::kWorkerSrc == effective_type)
    prefix = prefix + "create a worker from '";
  else if (ContentSecurityPolicy::DirectiveType::kConnectSrc == effective_type)
    prefix = prefix + "connect to '";
  else if (ContentSecurityPolicy::DirectiveType::kFontSrc == effective_type)
    prefix = prefix + "load the font '";
  else if (ContentSecurityPolicy::DirectiveType::kFormAction == effective_type)
    prefix = prefix + "send form data to '";
  else if (ContentSecurityPolicy::DirectiveType::kFrameSrc == effective_type)
    prefix = prefix + "frame '";
  else if (ContentSecurityPolicy::DirectiveType::kImgSrc == effective_type)
    prefix = prefix + "load the image '";
  else if (ContentSecurityPolicy::DirectiveType::kMediaSrc == effective_type)
    prefix = prefix + "load media from '";
  else if (ContentSecurityPolicy::DirectiveType::kManifestSrc == effective_type)
    prefix = prefix + "load manifest from '";
  else if (ContentSecurityPolicy::DirectiveType::kObjectSrc == effective_type)
    prefix = prefix + "load plugin data from '";
  else if (ContentSecurityPolicy::DirectiveType::kPrefetchSrc == effective_type)
    prefix = prefix + "prefetch content from '";
  else if (ContentSecurityPolicy::IsScriptDirective(effective_type))
    prefix = prefix + "load the script '";
  else if (ContentSecurityPolicy::IsStyleDirective(effective_type))
    prefix = prefix + "load the stylesheet '";
  else if (ContentSecurityPolicy::DirectiveType::kNavigateTo == effective_type)
    prefix = prefix + "navigate to '";

  String suffix = String();
  if (CheckDynamic(directive))
    suffix =
        " 'strict-dynamic' is present, so host-based whitelisting is disabled.";

  String directive_name = directive->GetName();
  String effective_directive_name =
      ContentSecurityPolicy::GetDirectiveName(effective_type);
  if (directive_name != effective_directive_name) {
    suffix = suffix + " Note that '" + effective_directive_name +
             "' was not explicitly set, so '" + directive_name +
             "' is used as a fallback.";
  }

  ReportViolation(directive->GetText(), effective_type,
                  prefix + url.ElidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      directive->GetText() + "\"." + suffix + "\n",
                  url, redirect_status);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::CheckAncestorsAndReportViolation(
    SourceListDirective* directive,
    LocalFrame* frame,
    const KURL& url) const {
  if (CheckAncestors(directive, frame))
    return true;

  ReportViolationWithFrame(
      directive->GetText(),
      ContentSecurityPolicy::DirectiveType::kFrameAncestors,
      "Refused to display '" + url.ElidedString() +
          "' in a frame because an ancestor violates the "
          "following Content Security Policy directive: "
          "\"" +
          directive->GetText() + "\".",
      url, frame);
  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowJavaScriptURLs(
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy) const {
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckInlineAndReportViolation(
        directive,
        "Refused to run the JavaScript URL because it violates the following "
        "Content Security Policy directive: ",
        element, source, context_url, context_line, true, "sha256-...",
        ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  }

  return !directive || directive->AllowAllInline();
}

bool CSPDirectiveList::AllowInlineEventHandlers(
    Element* element,
    const String& source,
    const String& context_url,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy) const {
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrcAttr);
  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckInlineAndReportViolation(
        directive,
        "Refused to execute inline event handler because it violates the "
        "following Content Security Policy directive: ",
        element, source, context_url, context_line, true, "sha256-...",
        ContentSecurityPolicy::DirectiveType::kScriptSrcAttr);
  }

  return !directive || directive->AllowAllInline();
}

bool CSPDirectiveList::AllowInlineScript(
    Element* element,
    const String& context_url,
    const String& nonce,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy,
    const String& content) const {
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  if (IsMatchingNoncePresent(directive, nonce))
    return true;
  if (element && IsHTMLScriptElement(element) &&
      !ToHTMLScriptElement(element)->Loader()->IsParserInserted() &&
      AllowDynamic(ContentSecurityPolicy::DirectiveType::kScriptSrcElem)) {
    return true;
  }
  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckInlineAndReportViolation(
        directive,
        "Refused to execute inline script because it violates the following "
        "Content Security Policy directive: ",
        element, content, context_url, context_line, true,
        GetSha256String(content),
        ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  }

  return !directive || directive->AllowAllInline();
}

bool CSPDirectiveList::AllowInlineStyle(
    Element* element,
    const String& context_url,
    const String& nonce,
    const WTF::OrdinalNumber& context_line,
    SecurityViolationReportingPolicy reporting_policy,
    const String& content,
    ContentSecurityPolicy::InlineType inline_type) const {
  ContentSecurityPolicy::DirectiveType effective_type =
      inline_type == ContentSecurityPolicy::InlineType::kAttribute
          ? ContentSecurityPolicy::DirectiveType::kStyleSrcAttr
          : ContentSecurityPolicy::DirectiveType::kStyleSrcElem;
  SourceListDirective* directive = OperativeDirective(effective_type);

  if (IsMatchingNoncePresent(directive, nonce))
    return true;

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckInlineAndReportViolation(
        directive,
        "Refused to apply inline style because it violates the following "
        "Content Security Policy directive: ",
        element, content, context_url, context_line, false,
        GetSha256String(content), effective_type);
  }

  return !directive || directive->AllowAllInline();
}

bool CSPDirectiveList::AllowEval(
    ScriptState* script_state,
    SecurityViolationReportingPolicy reporting_policy,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckEvalAndReportViolation(
        OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc),
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        script_state, exception_status, content);
  }
  return CheckEval(
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc));
}

bool CSPDirectiveList::AllowWasmEval(
    ScriptState* script_state,
    SecurityViolationReportingPolicy reporting_policy,
    ContentSecurityPolicy::ExceptionStatus exception_status,
    const String& content) const {
  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    return CheckWasmEvalAndReportViolation(
        OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc),
        "Refused to compile or instantiate WebAssembly module because "
        "'wasm-eval' is not an allowed source of script in the following "
        "Content Security Policy directive: ",
        script_state, exception_status, content);
  }
  return CheckWasmEval(
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrc));
}

bool CSPDirectiveList::AllowPluginType(
    const String& type,
    const String& type_attribute,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckMediaTypeAndReportViolation(
                   plugin_types_.Get(), type, type_attribute,
                   "Refused to load '" + url.ElidedString() + "' (MIME type '" +
                       type_attribute +
                       "') because it violates the following Content Security "
                       "Policy Directive: ")
             : CheckMediaType(plugin_types_.Get(), type, type_attribute);
}

bool CSPDirectiveList::AllowScriptFromSource(
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& hashes,
    ParserDisposition parser_disposition,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
  if (IsMatchingNoncePresent(directive, nonce))
    return true;
  if (parser_disposition == kNotParserInserted &&
      AllowDynamic(ContentSecurityPolicy::DirectiveType::kScriptSrcElem))
    return true;
  if (AreAllMatchingHashesPresent(directive, hashes))
    return true;
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   directive, url,
                   ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
                   redirect_status)
             : CheckSource(directive, url, redirect_status);
}

bool CSPDirectiveList::AllowObjectFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (url.ProtocolIsAbout())
    return true;
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kObjectSrc),
                   url, ContentSecurityPolicy::DirectiveType::kObjectSrc,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kObjectSrc),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowPrefetchFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kPrefetchSrc),
                   url, ContentSecurityPolicy::DirectiveType::kPrefetchSrc,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kPrefetchSrc),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowFrameFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (url.ProtocolIsAbout())
    return true;

  // 'frame-src' overrides 'child-src', which overrides the default
  // sources. So, we do this nested set of calls to 'operativeDirective()' to
  // grab 'frame-src' if it exists, 'child-src' if it doesn't, and 'defaut-src'
  // if neither are available.
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kFrameSrc);

  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   directive, url,
                   ContentSecurityPolicy::DirectiveType::kFrameSrc,
                   redirect_status)
             : CheckSource(directive, url, redirect_status);
}

bool CSPDirectiveList::AllowImageFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kImgSrc),
                   url, ContentSecurityPolicy::DirectiveType::kImgSrc,
                   redirect_status)
             : CheckSource(OperativeDirective(
                               ContentSecurityPolicy::DirectiveType::kImgSrc),
                           url, redirect_status);
}

bool CSPDirectiveList::AllowStyleFromSource(
    const KURL& url,
    const String& nonce,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  SourceListDirective* directive =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kStyleSrcElem);

  if (IsMatchingNoncePresent(directive, nonce))
    return true;
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   directive, url,
                   ContentSecurityPolicy::DirectiveType::kStyleSrcElem,
                   redirect_status)
             : CheckSource(directive, url, redirect_status);
}

bool CSPDirectiveList::AllowFontFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kFontSrc),
                   url, ContentSecurityPolicy::DirectiveType::kFontSrc,
                   redirect_status)
             : CheckSource(OperativeDirective(
                               ContentSecurityPolicy::DirectiveType::kFontSrc),
                           url, redirect_status);
}

bool CSPDirectiveList::AllowMediaFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kMediaSrc),
                   url, ContentSecurityPolicy::DirectiveType::kMediaSrc,
                   redirect_status)
             : CheckSource(OperativeDirective(
                               ContentSecurityPolicy::DirectiveType::kMediaSrc),
                           url, redirect_status);
}

bool CSPDirectiveList::AllowManifestFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kManifestSrc),
                   url, ContentSecurityPolicy::DirectiveType::kManifestSrc,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kManifestSrc),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowConnectToSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kConnectSrc),
                   url, ContentSecurityPolicy::DirectiveType::kConnectSrc,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kConnectSrc),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowFormAction(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kFormAction),
                   url, ContentSecurityPolicy::DirectiveType::kFormAction,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kFormAction),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowBaseURI(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  bool result =
      reporting_policy == SecurityViolationReportingPolicy::kReport
          ? CheckSourceAndReportViolation(
                OperativeDirective(
                    ContentSecurityPolicy::DirectiveType::kBaseURI),
                url, ContentSecurityPolicy::DirectiveType::kBaseURI,
                redirect_status)
          : CheckSource(OperativeDirective(
                            ContentSecurityPolicy::DirectiveType::kBaseURI),
                        url, redirect_status);

  if (result &&
      !CheckSource(
          OperativeDirective(ContentSecurityPolicy::DirectiveType::kBaseURI),
          url, redirect_status)) {
    UseCounter::Count(policy_->GetDocument(),
                      WebFeature::kBaseWouldBeBlockedByDefaultSrc);
  }

  return result;
}

bool CSPDirectiveList::AllowTrustedTypePolicy(const String& policy_name) const {
  if (!trusted_types_ || trusted_types_->Allows(policy_name))
    return true;

  ReportViolation(
      "trusted-types", ContentSecurityPolicy::DirectiveType::kTrustedTypes,
      String::Format(
          "Refused to create a TrustedTypePolicy named '%s' because "
          "it violates the following Content Security Policy directive: "
          "\"%s\".",
          policy_name.Utf8().data(),
          trusted_types_.Get()->GetText().Utf8().data()),
      KURL(), RedirectStatus::kNoRedirect);

  return DenyIfEnforcingPolicy();
}

bool CSPDirectiveList::AllowWorkerFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (AllowDynamicWorker())
    return true;

  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckSourceAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kWorkerSrc),
                   url, ContentSecurityPolicy::DirectiveType::kWorkerSrc,
                   redirect_status)
             : CheckSource(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kWorkerSrc),
                   url, redirect_status);
}

bool CSPDirectiveList::AllowAncestors(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  return reporting_policy == SecurityViolationReportingPolicy::kReport
             ? CheckAncestorsAndReportViolation(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kFrameAncestors),
                   frame, url)
             : CheckAncestors(
                   OperativeDirective(
                       ContentSecurityPolicy::DirectiveType::kFrameAncestors),
                   frame);
}

bool CSPDirectiveList::AllowHash(
    const CSPHashValue& hash_value,
    const ContentSecurityPolicy::InlineType type,
    const ContentSecurityPolicy::DirectiveType directive_type) const {
  if (type == ContentSecurityPolicy::InlineType::kAttribute) {
    if (!policy_->ExperimentalFeaturesEnabled())
      return false;
    if (!CheckUnsafeHashesAllowed(OperativeDirective(directive_type)))
      return false;
  }
  return CheckHash(OperativeDirective(directive_type), hash_value);
}

bool CSPDirectiveList::AllowScriptHash(
    const CSPHashValue& hash_value,
    ContentSecurityPolicy::InlineType type) const {
  return AllowHash(hash_value, type,
                   type == ContentSecurityPolicy::InlineType::kAttribute
                       ? ContentSecurityPolicy::DirectiveType::kScriptSrcAttr
                       : ContentSecurityPolicy::DirectiveType::kScriptSrcElem);
}

bool CSPDirectiveList::AllowStyleHash(
    const CSPHashValue& hash_value,
    ContentSecurityPolicy::InlineType type) const {
  return AllowHash(hash_value, type,
                   type == ContentSecurityPolicy::InlineType::kAttribute
                       ? ContentSecurityPolicy::DirectiveType::kStyleSrcAttr
                       : ContentSecurityPolicy::DirectiveType::kStyleSrcElem);
}

bool CSPDirectiveList::AllowDynamic(
    ContentSecurityPolicy::DirectiveType directive_type) const {
  return CheckDynamic(OperativeDirective(directive_type));
}

bool CSPDirectiveList::AllowDynamicWorker() const {
  SourceListDirective* worker_src =
      OperativeDirective(ContentSecurityPolicy::DirectiveType::kWorkerSrc);
  return CheckDynamic(worker_src);
}

const String& CSPDirectiveList::PluginTypesText() const {
  DCHECK(HasPluginTypes());
  return plugin_types_->GetText();
}

bool CSPDirectiveList::ShouldSendCSPHeader(ResourceType type) const {
  // TODO(mkwst): Revisit this once the CORS prefetch issue with the 'CSP'
  //              header is worked out, one way or another:
  //              https://github.com/whatwg/fetch/issues/52
  return false;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::Parse(const UChar* begin,
                             const UChar* end,
                             bool should_parse_wasm_eval) {
  header_ =
      String(begin, static_cast<wtf_size_t>(end - begin)).StripWhiteSpace();

  if (begin == end)
    return;

  const UChar* position = begin;
  while (position < end) {
    const UChar* directive_begin = position;
    SkipUntil<UChar>(position, end, ';');

    String name, value;
    if (ParseDirective(directive_begin, position, &name, &value)) {
      DCHECK(!name.IsEmpty());
      AddDirective(name, value);
    }

    DCHECK(position == end || *position == ';');
    SkipExactly<UChar>(position, end, ';');
  }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool CSPDirectiveList::ParseDirective(const UChar* begin,
                                      const UChar* end,
                                      String* name,
                                      String* value) {
  DCHECK(name->IsEmpty());
  DCHECK(value->IsEmpty());

  const UChar* position = begin;
  SkipWhile<UChar, IsASCIISpace>(position, end);

  // Empty directive (e.g. ";;;"). Exit early.
  if (position == end)
    return false;

  const UChar* name_begin = position;
  SkipWhile<UChar, IsCSPDirectiveNameCharacter>(position, end);

  // The directive-name must be non-empty.
  if (name_begin == position) {
    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy_->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  *name = String(name_begin, static_cast<wtf_size_t>(position - name_begin))
              .LowerASCII();

  if (position == end)
    return true;

  if (!SkipExactly<UChar, IsASCIISpace>(position, end)) {
    SkipWhile<UChar, IsNotASCIISpace>(position, end);
    policy_->ReportUnsupportedDirective(
        String(name_begin, static_cast<wtf_size_t>(position - name_begin)));
    return false;
  }

  SkipWhile<UChar, IsASCIISpace>(position, end);

  const UChar* value_begin = position;
  SkipWhile<UChar, IsCSPDirectiveValueCharacter>(position, end);

  if (position != end) {
    policy_->ReportInvalidDirectiveValueCharacter(
        *name, String(value_begin, static_cast<wtf_size_t>(end - value_begin)));
    return false;
  }

  // The directive-value may be empty.
  if (value_begin == position)
    return true;

  *value = String(value_begin, static_cast<wtf_size_t>(position - value_begin));
  return true;
}

void CSPDirectiveList::ParseRequireSRIFor(const String& name,
                                          const String& value) {
  if (require_sri_for_ != 0) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  StringBuilder token_errors;
  unsigned number_of_token_errors = 0;
  Vector<UChar> characters;
  value.AppendTo(characters);

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  while (position < end) {
    SkipWhile<UChar, IsASCIISpace>(position, end);

    const UChar* token_begin = position;
    SkipWhile<UChar, IsNotASCIISpace>(position, end);

    if (token_begin < position) {
      String token =
          String(token_begin, static_cast<wtf_size_t>(position - token_begin));
      if (EqualIgnoringASCIICase(token, "script")) {
        require_sri_for_ |= RequireSRIForToken::kScript;
      } else if (EqualIgnoringASCIICase(token, "style")) {
        require_sri_for_ |= RequireSRIForToken::kStyle;
      } else {
        if (number_of_token_errors)
          token_errors.Append(", \'");
        else
          token_errors.Append('\'');
        token_errors.Append(token);
        token_errors.Append('\'');
        number_of_token_errors++;
      }
    }
  }

  if (number_of_token_errors == 0)
    return;

  String invalid_tokens_error_message;
  if (number_of_token_errors > 1)
    token_errors.Append(" are invalid 'require-sri-for' tokens.");
  else
    token_errors.Append(" is an invalid 'require-sri-for' token.");

  invalid_tokens_error_message = token_errors.ToString();

  DCHECK(!invalid_tokens_error_message.IsEmpty());

  policy_->ReportInvalidRequireSRIForTokens(invalid_tokens_error_message);
}

void CSPDirectiveList::ParseReportTo(const String& name, const String& value) {
  if (!use_reporting_api_) {
    use_reporting_api_ = true;
    report_endpoints_.clear();
  }

  if (!report_endpoints_.IsEmpty()) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  ParseAndAppendReportEndpoints(value);
}

void CSPDirectiveList::ParseReportURI(const String& name, const String& value) {
  // report-to supersedes report-uri
  if (use_reporting_api_)
    return;

  if (!report_endpoints_.IsEmpty()) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  // Remove report-uri in meta policies, per
  // https://html.spec.whatwg.org/#attr-meta-http-equiv-content-security-policy.
  if (header_source_ == kContentSecurityPolicyHeaderSourceMeta) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }

  ParseAndAppendReportEndpoints(value);
}

void CSPDirectiveList::ParseAndAppendReportEndpoints(const String& value) {
  Vector<UChar> characters;
  value.AppendTo(characters);

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  while (position < end) {
    SkipWhile<UChar, IsASCIISpace>(position, end);

    const UChar* endpoint_begin = position;
    SkipWhile<UChar, IsNotASCIISpace>(position, end);

    if (endpoint_begin < position) {
      String endpoint = String(
          endpoint_begin, static_cast<wtf_size_t>(position - endpoint_begin));
      report_endpoints_.push_back(endpoint);
    }
  }

  if (report_endpoints_.size() > 1) {
    UseCounter::Count(policy_->GetDocument(),
                      WebFeature::kReportUriMultipleEndpoints);
  } else {
    UseCounter::Count(policy_->GetDocument(),
                      WebFeature::kReportUriSingleEndpoint);
  }
}

template <class CSPDirectiveType>
void CSPDirectiveList::SetCSPDirective(const String& name,
                                       const String& value,
                                       Member<CSPDirectiveType>& directive,
                                       bool should_parse_wasm_eval) {
  if (directive) {
    policy_->ReportDuplicateDirective(name);
    return;
  }

  // Remove frame-ancestors directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (header_source_ == kContentSecurityPolicyHeaderSourceMeta &&
      ContentSecurityPolicy::GetDirectiveType(name) ==
          ContentSecurityPolicy::DirectiveType::kFrameAncestors) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }

  directive = new CSPDirectiveType(name, value, policy_);
}

void CSPDirectiveList::ApplySandboxPolicy(const String& name,
                                          const String& sandbox_policy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (header_source_ == kContentSecurityPolicyHeaderSourceMeta) {
    policy_->ReportInvalidDirectiveInMeta(name);
    return;
  }
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (has_sandbox_policy_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  has_sandbox_policy_ = true;
  String invalid_tokens;
  SpaceSplitString policy_tokens =
      SpaceSplitString(AtomicString(sandbox_policy));
  policy_->EnforceSandboxFlags(
      ParseSandboxPolicy(policy_tokens, invalid_tokens));
  if (!invalid_tokens.IsNull())
    policy_->ReportInvalidSandboxFlags(invalid_tokens);
}

void CSPDirectiveList::TreatAsPublicAddress(const String& name,
                                            const String& value) {
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (treat_as_public_address_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  treat_as_public_address_ = true;
  policy_->TreatAsPublicAddress();
  if (!value.IsEmpty())
    policy_->ReportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::RequireTrustedTypes(const String& name,
                                           const String& value) {
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (trusted_types_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  policy_->RequireTrustedTypes();
  trusted_types_ = new StringListDirective(name, value, policy_);
}

void CSPDirectiveList::EnforceStrictMixedContentChecking(const String& name,
                                                         const String& value) {
  if (strict_mixed_content_checking_enforced_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  if (!value.IsEmpty())
    policy_->ReportValueForEmptyDirective(name, value);

  strict_mixed_content_checking_enforced_ = true;

  if (!IsReportOnly())
    policy_->EnforceStrictMixedContentChecking();
}

void CSPDirectiveList::EnableInsecureRequestsUpgrade(const String& name,
                                                     const String& value) {
  if (IsReportOnly()) {
    policy_->ReportInvalidInReportOnly(name);
    return;
  }
  if (upgrade_insecure_requests_) {
    policy_->ReportDuplicateDirective(name);
    return;
  }
  upgrade_insecure_requests_ = true;

  policy_->UpgradeInsecureRequests();
  if (!value.IsEmpty())
    policy_->ReportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::AddDirective(const String& name, const String& value) {
  DCHECK(!name.IsEmpty());

  ContentSecurityPolicy::DirectiveType type =
      ContentSecurityPolicy::GetDirectiveType(name);
  if (type == ContentSecurityPolicy::DirectiveType::kDefaultSrc) {
    SetCSPDirective<SourceListDirective>(name, value, default_src_);
    // TODO(mkwst) It seems unlikely that developers would use different
    // algorithms for scripts and styles. We may want to combine the
    // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
    policy_->UsesScriptHashAlgorithms(default_src_->HashAlgorithmsUsed());
    policy_->UsesStyleHashAlgorithms(default_src_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kScriptSrc) {
    SetCSPDirective<SourceListDirective>(name, value, script_src_);
    policy_->UsesScriptHashAlgorithms(script_src_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kScriptSrcAttr &&
             policy_->ExperimentalFeaturesEnabled()) {
    SetCSPDirective<SourceListDirective>(name, value, script_src_attr_);
    policy_->UsesScriptHashAlgorithms(script_src_attr_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kScriptSrcElem &&
             policy_->ExperimentalFeaturesEnabled()) {
    SetCSPDirective<SourceListDirective>(name, value, script_src_elem_);
    policy_->UsesScriptHashAlgorithms(script_src_elem_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kObjectSrc) {
    SetCSPDirective<SourceListDirective>(name, value, object_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kFrameAncestors) {
    SetCSPDirective<SourceListDirective>(name, value, frame_ancestors_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kFrameSrc) {
    SetCSPDirective<SourceListDirective>(name, value, frame_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kImgSrc) {
    SetCSPDirective<SourceListDirective>(name, value, img_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kStyleSrc) {
    SetCSPDirective<SourceListDirective>(name, value, style_src_);
    policy_->UsesStyleHashAlgorithms(style_src_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kStyleSrcAttr &&
             policy_->ExperimentalFeaturesEnabled()) {
    SetCSPDirective<SourceListDirective>(name, value, style_src_attr_);
    policy_->UsesStyleHashAlgorithms(style_src_attr_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kStyleSrcElem &&
             policy_->ExperimentalFeaturesEnabled()) {
    SetCSPDirective<SourceListDirective>(name, value, style_src_elem_);
    policy_->UsesStyleHashAlgorithms(style_src_elem_->HashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::kFontSrc) {
    SetCSPDirective<SourceListDirective>(name, value, font_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kMediaSrc) {
    SetCSPDirective<SourceListDirective>(name, value, media_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kConnectSrc) {
    SetCSPDirective<SourceListDirective>(name, value, connect_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kSandbox) {
    ApplySandboxPolicy(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::kReportURI) {
    ParseReportURI(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::kBaseURI) {
    SetCSPDirective<SourceListDirective>(name, value, base_uri_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kChildSrc) {
    SetCSPDirective<SourceListDirective>(name, value, child_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kWorkerSrc) {
    SetCSPDirective<SourceListDirective>(name, value, worker_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kFormAction) {
    SetCSPDirective<SourceListDirective>(name, value, form_action_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kPluginTypes) {
    SetCSPDirective<MediaListDirective>(name, value, plugin_types_);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::kUpgradeInsecureRequests) {
    EnableInsecureRequestsUpgrade(name, value);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::kBlockAllMixedContent) {
    EnforceStrictMixedContentChecking(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::kManifestSrc) {
    SetCSPDirective<SourceListDirective>(name, value, manifest_src_);
  } else if (type == ContentSecurityPolicy::DirectiveType::kNavigateTo) {
    SetCSPDirective<SourceListDirective>(name, value, navigate_to_);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::kTreatAsPublicAddress) {
    TreatAsPublicAddress(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::kReportTo &&
             base::FeatureList::IsEnabled(network::features::kReporting)) {
    ParseReportTo(name, value);
  } else if (policy_->ExperimentalFeaturesEnabled()) {
    if (type == ContentSecurityPolicy::DirectiveType::kRequireSRIFor) {
      ParseRequireSRIFor(name, value);
    } else if (type == ContentSecurityPolicy::DirectiveType::kTrustedTypes &&
               RuntimeEnabledFeatures::TrustedDOMTypesEnabled()) {
      RequireTrustedTypes(name, value);
    } else if (type == ContentSecurityPolicy::DirectiveType::kPrefetchSrc) {
      SetCSPDirective<SourceListDirective>(name, value, prefetch_src_);
    } else {
      policy_->ReportUnsupportedDirective(name);
    }
  } else {
    policy_->ReportUnsupportedDirective(name);
  }
}

ContentSecurityPolicy::DirectiveType CSPDirectiveList::FallbackDirective(
    const ContentSecurityPolicy::DirectiveType current_directive,
    const ContentSecurityPolicy::DirectiveType original_directive) const {
  switch (current_directive) {
    case ContentSecurityPolicy::DirectiveType::kConnectSrc:
    case ContentSecurityPolicy::DirectiveType::kFontSrc:
    case ContentSecurityPolicy::DirectiveType::kImgSrc:
    case ContentSecurityPolicy::DirectiveType::kManifestSrc:
    case ContentSecurityPolicy::DirectiveType::kMediaSrc:
    case ContentSecurityPolicy::DirectiveType::kPrefetchSrc:
    case ContentSecurityPolicy::DirectiveType::kObjectSrc:
    case ContentSecurityPolicy::DirectiveType::kScriptSrc:
    case ContentSecurityPolicy::DirectiveType::kStyleSrc:
      return ContentSecurityPolicy::DirectiveType::kDefaultSrc;

    case ContentSecurityPolicy::DirectiveType::kScriptSrcAttr:
    case ContentSecurityPolicy::DirectiveType::kScriptSrcElem:
      return ContentSecurityPolicy::DirectiveType::kScriptSrc;

    case ContentSecurityPolicy::DirectiveType::kStyleSrcAttr:
    case ContentSecurityPolicy::DirectiveType::kStyleSrcElem:
      return ContentSecurityPolicy::DirectiveType::kStyleSrc;

    case ContentSecurityPolicy::DirectiveType::kFrameSrc:
    case ContentSecurityPolicy::DirectiveType::kWorkerSrc:
      return ContentSecurityPolicy::DirectiveType::kChildSrc;

    // Because the fallback chain of child-src can be different if we are
    // checking a worker or a frame request, we need to know the original type
    // of the request to decide. These are the fallback chains for worker-src
    // and frame-src specifically.

    // worker-src > child-src > script-src > default-src
    // frame-src > child-src > default-src

    // Since there are some situations and tests that will operate on the
    // `child-src` directive directly (like for example the EE subsumption
    // algorithm), we consider the child-src > default-src fallback path as the
    // "default" and the worker-src fallback path as an exception.
    case ContentSecurityPolicy::DirectiveType::kChildSrc:
      if (original_directive ==
          ContentSecurityPolicy::DirectiveType::kWorkerSrc)
        return ContentSecurityPolicy::DirectiveType::kScriptSrc;

      return ContentSecurityPolicy::DirectiveType::kDefaultSrc;

    default:
      return ContentSecurityPolicy::DirectiveType::kUndefined;
  }
}

SourceListDirective* CSPDirectiveList::OperativeDirective(
    const ContentSecurityPolicy::DirectiveType type,
    ContentSecurityPolicy::DirectiveType original_type) const {
  if (type == ContentSecurityPolicy::DirectiveType::kUndefined) {
    return nullptr;
  }

  SourceListDirective* directive;
  if (original_type == ContentSecurityPolicy::DirectiveType::kUndefined) {
    original_type = type;
  }

  switch (type) {
    case ContentSecurityPolicy::DirectiveType::kBaseURI:
      directive = base_uri_;
      break;
    case ContentSecurityPolicy::DirectiveType::kDefaultSrc:
      directive = default_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFrameAncestors:
      directive = frame_ancestors_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFormAction:
      directive = form_action_;
      break;
    case ContentSecurityPolicy::DirectiveType::kNavigateTo:
      directive = navigate_to_;
      break;
    case ContentSecurityPolicy::DirectiveType::kChildSrc:
      directive = child_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kConnectSrc:
      directive = connect_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFontSrc:
      directive = font_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kImgSrc:
      directive = img_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kManifestSrc:
      directive = manifest_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kMediaSrc:
      directive = media_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kObjectSrc:
      directive = object_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kPrefetchSrc:
      directive = prefetch_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrc:
      directive = script_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcAttr:
      directive = policy_->ExperimentalFeaturesEnabled() ? script_src_attr_
                                                         : script_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kScriptSrcElem:
      directive = policy_->ExperimentalFeaturesEnabled() ? script_src_elem_
                                                         : script_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrc:
      directive = style_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcAttr:
      directive =
          policy_->ExperimentalFeaturesEnabled() ? style_src_attr_ : style_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kStyleSrcElem:
      directive =
          policy_->ExperimentalFeaturesEnabled() ? style_src_elem_ : style_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kFrameSrc:
      directive = frame_src_;
      break;
    case ContentSecurityPolicy::DirectiveType::kWorkerSrc:
      directive = worker_src_;
      break;
    default:
      return nullptr;
  }

  // if the directive does not exist, rely on the fallback directive
  return directive ? directive
                   : OperativeDirective(FallbackDirective(type, original_type),
                                        original_type);
}

SourceListDirectiveVector CSPDirectiveList::GetSourceVector(
    const ContentSecurityPolicy::DirectiveType type,
    const CSPDirectiveListVector& policies) {
  SourceListDirectiveVector source_list_directives;
  for (const auto& policy : policies) {
    if (SourceListDirective* directive = policy->OperativeDirective(type)) {
      if (directive->IsNone())
        return SourceListDirectiveVector(1, directive);
      source_list_directives.push_back(directive);
    }
  }

  return source_list_directives;
}

bool CSPDirectiveList::Subsumes(const CSPDirectiveListVector& other) {
  // A white-list of directives that we consider for subsumption.
  // See more about source lists here:
  // https://w3c.github.io/webappsec-csp/#framework-directive-source-list
  static ContentSecurityPolicy::DirectiveType directives[] = {
      ContentSecurityPolicy::DirectiveType::kChildSrc,
      ContentSecurityPolicy::DirectiveType::kConnectSrc,
      ContentSecurityPolicy::DirectiveType::kFontSrc,
      ContentSecurityPolicy::DirectiveType::kFrameSrc,
      ContentSecurityPolicy::DirectiveType::kImgSrc,
      ContentSecurityPolicy::DirectiveType::kManifestSrc,
      ContentSecurityPolicy::DirectiveType::kMediaSrc,
      ContentSecurityPolicy::DirectiveType::kObjectSrc,
      ContentSecurityPolicy::DirectiveType::kScriptSrc,
      ContentSecurityPolicy::DirectiveType::kScriptSrcAttr,
      ContentSecurityPolicy::DirectiveType::kScriptSrcElem,
      ContentSecurityPolicy::DirectiveType::kStyleSrc,
      ContentSecurityPolicy::DirectiveType::kStyleSrcAttr,
      ContentSecurityPolicy::DirectiveType::kStyleSrcElem,
      ContentSecurityPolicy::DirectiveType::kWorkerSrc,
      ContentSecurityPolicy::DirectiveType::kBaseURI,
      ContentSecurityPolicy::DirectiveType::kFrameAncestors,
      ContentSecurityPolicy::DirectiveType::kFormAction,
      ContentSecurityPolicy::DirectiveType::kNavigateTo};

  for (const auto& directive : directives) {
    // There should only be one SourceListDirective for each directive in
    // Embedding-CSP.
    SourceListDirectiveVector required_list =
        GetSourceVector(directive, CSPDirectiveListVector(1, this));
    if (!required_list.size())
      continue;
    SourceListDirective* required = required_list[0];
    // Aggregate all serialized source lists of the returned CSP into a vector
    // based on a directive type, defaulting accordingly (for example, to
    // `default-src`).
    SourceListDirectiveVector returned = GetSourceVector(directive, other);
    // TODO(amalika): Add checks for plugin-types, sandbox, disown-opener,
    // navigation-to, worker-src.
    if (!required->Subsumes(returned))
      return false;
  }

  if (!HasPluginTypes())
    return true;

  HeapVector<Member<MediaListDirective>> plugin_types_other;
  for (const auto& policy : other) {
    if (policy->HasPluginTypes())
      plugin_types_other.push_back(policy->plugin_types_);
  }

  return plugin_types_->Subsumes(plugin_types_other);
}

WebContentSecurityPolicy CSPDirectiveList::ExposeForNavigationalChecks() const {
  WebContentSecurityPolicy policy;
  policy.disposition = static_cast<WebContentSecurityPolicyType>(header_type_);
  policy.source = static_cast<WebContentSecurityPolicySource>(header_source_);
  std::vector<WebContentSecurityPolicyDirective> directives;
  for (const auto& directive :
       {child_src_, default_src_, form_action_, frame_src_, navigate_to_}) {
    if (directive) {
      directives.push_back(WebContentSecurityPolicyDirective{
          directive->DirectiveName(),
          directive->ExposeForNavigationalChecks()});
    }
  }
  if (upgrade_insecure_requests_) {
    directives.push_back(WebContentSecurityPolicyDirective{
        blink::WebString("upgrade-insecure-requests"),
        WebContentSecurityPolicySourceList()});
  }
  policy.directives = directives;

  std::vector<WebString> report_endpoints;
  for (const auto& report_endpoint : ReportEndpoints()) {
    report_endpoints.push_back(report_endpoint);
  }

  policy.use_reporting_api = use_reporting_api_;

  policy.report_endpoints = report_endpoints;

  policy.header = Header();

  return policy;
}

void CSPDirectiveList::Trace(blink::Visitor* visitor) {
  visitor->Trace(policy_);
  visitor->Trace(plugin_types_);
  visitor->Trace(base_uri_);
  visitor->Trace(child_src_);
  visitor->Trace(connect_src_);
  visitor->Trace(default_src_);
  visitor->Trace(font_src_);
  visitor->Trace(form_action_);
  visitor->Trace(frame_ancestors_);
  visitor->Trace(frame_src_);
  visitor->Trace(img_src_);
  visitor->Trace(media_src_);
  visitor->Trace(manifest_src_);
  visitor->Trace(object_src_);
  visitor->Trace(prefetch_src_);
  visitor->Trace(script_src_);
  visitor->Trace(script_src_attr_);
  visitor->Trace(script_src_elem_);
  visitor->Trace(style_src_);
  visitor->Trace(style_src_attr_);
  visitor->Trace(style_src_elem_);
  visitor->Trace(worker_src_);
  visitor->Trace(navigate_to_);
  visitor->Trace(trusted_types_);
}

}  // namespace blink
