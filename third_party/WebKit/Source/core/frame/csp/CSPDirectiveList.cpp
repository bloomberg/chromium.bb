// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPDirectiveList.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/SpaceSplitString.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLScriptElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Crypto.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/Base64.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

String getSha256String(const String& content) {
  DigestValue digest;
  StringUTF8Adaptor utf8Content(content);
  bool digestSuccess = computeDigest(HashAlgorithmSha256, utf8Content.data(),
                                     utf8Content.length(), digest);
  if (!digestSuccess) {
    return "sha256-...";
  }

  return "sha256-" + base64Encode(reinterpret_cast<char*>(digest.data()),
                                  digest.size(), Base64DoNotInsertLFs);
}

template <typename CharType>
inline bool isASCIIAlphanumericOrHyphen(CharType c) {
  return isASCIIAlphanumeric(c) || c == '-';
}

}  // namespace

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy,
                                   ContentSecurityPolicyHeaderType type,
                                   ContentSecurityPolicyHeaderSource source)
    : m_policy(policy),
      m_headerType(type),
      m_headerSource(source),
      m_hasSandboxPolicy(false),
      m_strictMixedContentCheckingEnforced(false),
      m_upgradeInsecureRequests(false),
      m_treatAsPublicAddress(false),
      m_requireSRIFor(RequireSRIForToken::None) {}

CSPDirectiveList* CSPDirectiveList::create(
    ContentSecurityPolicy* policy,
    const UChar* begin,
    const UChar* end,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  CSPDirectiveList* directives = new CSPDirectiveList(policy, type, source);
  directives->parse(begin, end);

  if (!directives->checkEval(
          directives->operativeDirective(directives->m_scriptSrc.get()))) {
    String message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        directives->operativeDirective(directives->m_scriptSrc.get())->text() +
        "\".\n";
    directives->setEvalDisabledErrorMessage(message);
  }

  if (directives->isReportOnly() &&
      source != ContentSecurityPolicyHeaderSourceMeta &&
      directives->reportEndpoints().isEmpty())
    policy->reportMissingReportURI(String(begin, end - begin));

  return directives;
}

void CSPDirectiveList::reportViolation(
    const String& directiveText,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    const String& consoleMessage,
    const KURL& blockedURL,
    ResourceRequest::RedirectStatus redirectStatus) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(ConsoleMessage::create(SecurityMessageSource,
                                                ErrorMessageLevel, message));
  m_policy->reportViolation(directiveText, effectiveType, message, blockedURL,
                            m_reportEndpoints, m_header, m_headerType,
                            ContentSecurityPolicy::URLViolation, nullptr,
                            redirectStatus);
}

void CSPDirectiveList::reportViolationWithFrame(
    const String& directiveText,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    const String& consoleMessage,
    const KURL& blockedURL,
    LocalFrame* frame) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(
      ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message),
      frame);
  m_policy->reportViolation(directiveText, effectiveType, message, blockedURL,
                            m_reportEndpoints, m_header, m_headerType,
                            ContentSecurityPolicy::URLViolation, frame);
}

void CSPDirectiveList::reportViolationWithLocation(
    const String& directiveText,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    const String& consoleMessage,
    const KURL& blockedURL,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    Element* element,
    const String& source) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(ConsoleMessage::create(
      SecurityMessageSource, ErrorMessageLevel, message,
      SourceLocation::capture(contextURL, contextLine.oneBasedInt(), 0)));
  m_policy->reportViolation(
      directiveText, effectiveType, message, blockedURL, m_reportEndpoints,
      m_header, m_headerType, ContentSecurityPolicy::InlineViolation, nullptr,
      RedirectStatus::NoRedirect, contextLine.oneBasedInt(), element, source);
}

void CSPDirectiveList::reportViolationWithState(
    const String& directiveText,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    const String& message,
    const KURL& blockedURL,
    ScriptState* scriptState,
    const ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  String reportMessage = isReportOnly() ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a
  // JavaScript exception that the caller will throw. (Exceptions will
  // never get thrown in report-only mode because the caller won't see
  // a violation.)
  if (isReportOnly() ||
      exceptionStatus == ContentSecurityPolicy::WillNotThrowException) {
    ConsoleMessage* consoleMessage = ConsoleMessage::create(
        SecurityMessageSource, ErrorMessageLevel, reportMessage);
    m_policy->logToConsole(consoleMessage);
  }
  m_policy->reportViolation(directiveText, effectiveType, message, blockedURL,
                            m_reportEndpoints, m_header, m_headerType,
                            ContentSecurityPolicy::EvalViolation);
}

bool CSPDirectiveList::checkEval(SourceListDirective* directive) const {
  return !directive || directive->allowEval();
}

bool CSPDirectiveList::isMatchingNoncePresent(SourceListDirective* directive,
                                              const String& nonce) const {
  return directive && directive->allowNonce(nonce);
}

bool CSPDirectiveList::checkHash(SourceListDirective* directive,
                                 const CSPHashValue& hashValue) const {
  return !directive || directive->allowHash(hashValue);
}

bool CSPDirectiveList::checkHashedAttributes(
    SourceListDirective* directive) const {
  return !directive || directive->allowHashedAttributes();
}

bool CSPDirectiveList::checkDynamic(SourceListDirective* directive) const {
  return !directive || directive->allowDynamic();
}

void CSPDirectiveList::reportMixedContent(
    const KURL& mixedURL,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (strictMixedContentChecking()) {
    m_policy->reportViolation(
        ContentSecurityPolicy::getDirectiveName(
            ContentSecurityPolicy::DirectiveType::BlockAllMixedContent),
        ContentSecurityPolicy::DirectiveType::BlockAllMixedContent, String(),
        mixedURL, m_reportEndpoints, m_header, m_headerType,
        ContentSecurityPolicy::URLViolation, nullptr, redirectStatus);
  }
}

bool CSPDirectiveList::checkSource(
    SourceListDirective* directive,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  return !directive ||
         directive->allows(url.isEmpty() ? m_policy->url() : url,
                           redirectStatus);
}

bool CSPDirectiveList::checkAncestors(SourceListDirective* directive,
                                      LocalFrame* frame) const {
  if (!frame || !directive)
    return true;

  for (Frame* current = frame->tree().parent(); current;
       current = current->tree().parent()) {
    // The |current| frame might be a remote frame which has no URL, so use
    // its origin instead.  This should suffice for this check since it
    // doesn't do path comparisons.  See https://crbug.com/582544.
    //
    // TODO(mkwst): Move this check up into the browser process.  See
    // https://crbug.com/555418.
    KURL url(KURL(),
             current->securityContext()->getSecurityOrigin()->toString());
    if (!directive->allows(url, ResourceRequest::RedirectStatus::NoRedirect))
      return false;
  }
  return true;
}

bool CSPDirectiveList::checkRequestWithoutIntegrity(
    WebURLRequest::RequestContext context) const {
  if (m_requireSRIFor == RequireSRIForToken::None)
    return true;
  // SRI specification
  // (https://w3c.github.io/webappsec-subresource-integrity/#apply-algorithm-to-request)
  // says to match token with request's destination with the token.
  // Keep this logic aligned with ContentSecurityPolicy::allowRequest
  if ((m_requireSRIFor & RequireSRIForToken::Script) &&
      (context == WebURLRequest::RequestContextScript ||
       context == WebURLRequest::RequestContextImport ||
       context == WebURLRequest::RequestContextServiceWorker ||
       context == WebURLRequest::RequestContextSharedWorker ||
       context == WebURLRequest::RequestContextWorker)) {
    return false;
  }
  if ((m_requireSRIFor & RequireSRIForToken::Style) &&
      context == WebURLRequest::RequestContextStyle)
    return false;
  return true;
}

bool CSPDirectiveList::checkRequestWithoutIntegrityAndReportViolation(
    WebURLRequest::RequestContext context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (checkRequestWithoutIntegrity(context))
    return true;
  String resourceType;
  switch (context) {
    case WebURLRequest::RequestContextScript:
    case WebURLRequest::RequestContextImport:
      resourceType = "script";
      break;
    case WebURLRequest::RequestContextStyle:
      resourceType = "stylesheet";
      break;
    case WebURLRequest::RequestContextServiceWorker:
      resourceType = "service worker";
      break;
    case WebURLRequest::RequestContextSharedWorker:
      resourceType = "shared worker";
      break;
    case WebURLRequest::RequestContextWorker:
      resourceType = "worker";
      break;
    default:
      break;
  }

  reportViolation(ContentSecurityPolicy::getDirectiveName(
                      ContentSecurityPolicy::DirectiveType::RequireSRIFor),
                  ContentSecurityPolicy::DirectiveType::RequireSRIFor,
                  "Refused to load the " + resourceType + " '" +
                      url.elidedString() +
                      "' because 'require-sri-for' directive requires "
                      "integrity attribute be present for all " +
                      resourceType + "s.",
                  url, redirectStatus);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowRequestWithoutIntegrity(
    WebURLRequest::RequestContext context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  if (reportingPolicy == SecurityViolationReportingPolicy::Report)
    return checkRequestWithoutIntegrityAndReportViolation(context, url,
                                                          redirectStatus);
  return denyIfEnforcingPolicy() || checkRequestWithoutIntegrity(context);
}

bool CSPDirectiveList::checkMediaType(MediaListDirective* directive,
                                      const String& type,
                                      const String& typeAttribute) const {
  if (!directive)
    return true;
  if (typeAttribute.isEmpty() || typeAttribute.stripWhiteSpace() != type)
    return false;
  return directive->allows(type);
}

SourceListDirective* CSPDirectiveList::operativeDirective(
    SourceListDirective* directive) const {
  return directive ? directive : m_defaultSrc.get();
}

SourceListDirective* CSPDirectiveList::operativeDirective(
    SourceListDirective* directive,
    SourceListDirective* override) const {
  return directive ? directive : override;
}

bool CSPDirectiveList::checkEvalAndReportViolation(
    SourceListDirective* directive,
    const String& consoleMessage,
    ScriptState* scriptState,
    ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  if (checkEval(directive))
    return true;

  String suffix = String();
  if (directive == m_defaultSrc)
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";

  reportViolationWithState(
      directive->text(), ContentSecurityPolicy::DirectiveType::ScriptSrc,
      consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(),
      scriptState, exceptionStatus);
  if (!isReportOnly()) {
    m_policy->reportBlockedScriptExecutionToInspector(directive->text());
    return false;
  }
  return true;
}

bool CSPDirectiveList::checkMediaTypeAndReportViolation(
    MediaListDirective* directive,
    const String& type,
    const String& typeAttribute,
    const String& consoleMessage) const {
  if (checkMediaType(directive, type, typeAttribute))
    return true;

  String message = consoleMessage + "\'" + directive->text() + "\'.";
  if (typeAttribute.isEmpty())
    message = message +
              " When enforcing the 'plugin-types' directive, the plugin's "
              "media type must be explicitly declared with a 'type' attribute "
              "on the containing element (e.g. '<object type=\"[TYPE GOES "
              "HERE]\" ...>').";

  // 'RedirectStatus::NoRedirect' is safe here, as we do the media type check
  // before actually loading data; this means that we shouldn't leak redirect
  // targets, as we won't have had a chance to redirect yet.
  reportViolation(
      directive->text(), ContentSecurityPolicy::DirectiveType::PluginTypes,
      message + "\n", KURL(), ResourceRequest::RedirectStatus::NoRedirect);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkInlineAndReportViolation(
    SourceListDirective* directive,
    const String& consoleMessage,
    Element* element,
    const String& source,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    bool isScript,
    const String& hashValue) const {
  if (!directive || directive->allowAllInline())
    return true;

  String suffix = String();
  if (directive->allowInline() && directive->isHashOrNoncePresent()) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash ('" + hashValue +
        "'), or a nonce ('nonce-...') is required to enable inline execution.";
    if (directive == m_defaultSrc)
      suffix = suffix + " Note also that '" +
               String(isScript ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  reportViolationWithLocation(
      directive->text(),
      isScript ? ContentSecurityPolicy::DirectiveType::ScriptSrc
               : ContentSecurityPolicy::DirectiveType::StyleSrc,
      consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(),
      contextURL, contextLine, element,
      directive->allowReportSample() ? source : emptyString);

  if (!isReportOnly()) {
    if (isScript)
      m_policy->reportBlockedScriptExecutionToInspector(directive->text());
    return false;
  }
  return true;
}

bool CSPDirectiveList::checkSourceAndReportViolation(
    SourceListDirective* directive,
    const KURL& url,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (!directive)
    return true;

  // We ignore URL-based whitelists if we're allowing dynamic script injection.
  if (checkSource(directive, url, redirectStatus) && !checkDynamic(directive))
    return true;

  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::ChildSrc, effectiveType);
  DCHECK_NE(ContentSecurityPolicy::DirectiveType::DefaultSrc, effectiveType);

  String prefix;
  if (ContentSecurityPolicy::DirectiveType::BaseURI == effectiveType)
    prefix = "Refused to set the document's base URI to '";
  else if (ContentSecurityPolicy::DirectiveType::WorkerSrc == effectiveType)
    prefix = "Refused to create a worker from '";
  else if (ContentSecurityPolicy::DirectiveType::ConnectSrc == effectiveType)
    prefix = "Refused to connect to '";
  else if (ContentSecurityPolicy::DirectiveType::FontSrc == effectiveType)
    prefix = "Refused to load the font '";
  else if (ContentSecurityPolicy::DirectiveType::FormAction == effectiveType)
    prefix = "Refused to send form data to '";
  else if (ContentSecurityPolicy::DirectiveType::FrameSrc == effectiveType)
    prefix = "Refused to frame '";
  else if (ContentSecurityPolicy::DirectiveType::ImgSrc == effectiveType)
    prefix = "Refused to load the image '";
  else if (ContentSecurityPolicy::DirectiveType::MediaSrc == effectiveType)
    prefix = "Refused to load media from '";
  else if (ContentSecurityPolicy::DirectiveType::ManifestSrc == effectiveType)
    prefix = "Refused to load manifest from '";
  else if (ContentSecurityPolicy::DirectiveType::ObjectSrc == effectiveType)
    prefix = "Refused to load plugin data from '";
  else if (ContentSecurityPolicy::DirectiveType::ScriptSrc == effectiveType)
    prefix = "Refused to load the script '";
  else if (ContentSecurityPolicy::DirectiveType::StyleSrc == effectiveType)
    prefix = "Refused to load the stylesheet '";

  String suffix = String();
  if (checkDynamic(directive))
    suffix =
        " 'strict-dynamic' is present, so host-based whitelisting is disabled.";

  String directiveName = directive->name();
  String effectiveDirectiveName =
      ContentSecurityPolicy::getDirectiveName(effectiveType);
  if (directiveName != effectiveDirectiveName) {
    suffix = suffix + " Note that '" + effectiveDirectiveName +
             "' was not explicitly set, so '" + directiveName +
             "' is used as a fallback.";
  }

  reportViolation(directive->text(), effectiveType,
                  prefix + url.elidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      directive->text() + "\"." + suffix + "\n",
                  url, redirectStatus);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkAncestorsAndReportViolation(
    SourceListDirective* directive,
    LocalFrame* frame,
    const KURL& url) const {
  if (checkAncestors(directive, frame))
    return true;

  reportViolationWithFrame(directive->text(),
                           ContentSecurityPolicy::DirectiveType::FrameAncestors,
                           "Refused to display '" + url.elidedString() +
                               "' in a frame because an ancestor violates the "
                               "following Content Security Policy directive: "
                               "\"" +
                               directive->text() + "\".",
                           url, frame);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowJavaScriptURLs(
    Element* element,
    const String& source,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    SecurityViolationReportingPolicy reportingPolicy) const {
  SourceListDirective* directive = operativeDirective(m_scriptSrc.get());
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    return checkInlineAndReportViolation(
        directive,
        "Refused to execute JavaScript URL because it violates the following "
        "Content Security Policy directive: ",
        element, source, contextURL, contextLine, true, "sha256-...");
  }

  return !directive || directive->allowAllInline();
}

bool CSPDirectiveList::allowInlineEventHandlers(
    Element* element,
    const String& source,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    SecurityViolationReportingPolicy reportingPolicy) const {
  SourceListDirective* directive = operativeDirective(m_scriptSrc.get());
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    return checkInlineAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to execute inline event handler because it violates the "
        "following Content Security Policy directive: ",
        element, source, contextURL, contextLine, true, "sha256-...");
  }

  return !directive || directive->allowAllInline();
}

bool CSPDirectiveList::allowInlineScript(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    SecurityViolationReportingPolicy reportingPolicy,
    const String& content) const {
  SourceListDirective* directive = operativeDirective(m_scriptSrc.get());
  if (isMatchingNoncePresent(directive, nonce))
    return true;
  if (element && isHTMLScriptElement(element) &&
      !toHTMLScriptElement(element)->loader()->isParserInserted() &&
      allowDynamic()) {
    return true;
  }
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    return checkInlineAndReportViolation(
        directive,
        "Refused to execute inline script because it violates the following "
        "Content Security Policy directive: ",
        element, content, contextURL, contextLine, true,
        getSha256String(content));
  }

  return !directive || directive->allowAllInline();
}

bool CSPDirectiveList::allowInlineStyle(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    SecurityViolationReportingPolicy reportingPolicy,
    const String& content) const {
  SourceListDirective* directive = operativeDirective(m_styleSrc.get());
  if (isMatchingNoncePresent(directive, nonce))
    return true;
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    return checkInlineAndReportViolation(
        directive,
        "Refused to apply inline style because it violates the following "
        "Content Security Policy directive: ",
        element, content, contextURL, contextLine, false,
        getSha256String(content));
  }

  return !directive || directive->allowAllInline();
}

bool CSPDirectiveList::allowEval(
    ScriptState* scriptState,
    SecurityViolationReportingPolicy reportingPolicy,
    ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    return checkEvalAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        scriptState, exceptionStatus);
  }
  return checkEval(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowPluginType(
    const String& type,
    const String& typeAttribute,
    const KURL& url,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkMediaTypeAndReportViolation(
                   m_pluginTypes.get(), type, typeAttribute,
                   "Refused to load '" + url.elidedString() + "' (MIME type '" +
                       typeAttribute +
                       "') because it violates the following Content Security "
                       "Policy Directive: ")
             : checkMediaType(m_pluginTypes.get(), type, typeAttribute);
}

bool CSPDirectiveList::allowScriptFromSource(
    const KURL& url,
    const String& nonce,
    ParserDisposition parserDisposition,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  if (isMatchingNoncePresent(operativeDirective(m_scriptSrc.get()), nonce))
    return true;
  if (parserDisposition == NotParserInserted && allowDynamic())
    return true;
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_scriptSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::ScriptSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_scriptSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowObjectFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  if (url.protocolIsAbout())
    return true;
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_objectSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::ObjectSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_objectSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFrameFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  if (url.protocolIsAbout())
    return true;

  // 'frame-src' overrides 'child-src', which overrides the default
  // sources. So, we do this nested set of calls to 'operativeDirective()' to
  // grab 'frame-src' if it exists, 'child-src' if it doesn't, and 'defaut-src'
  // if neither are available.
  SourceListDirective* whichDirective = operativeDirective(
      m_frameSrc.get(), operativeDirective(m_childSrc.get()));

  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   whichDirective, url,
                   ContentSecurityPolicy::DirectiveType::FrameSrc,
                   redirectStatus)
             : checkSource(whichDirective, url, redirectStatus);
}

bool CSPDirectiveList::allowImageFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_imgSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::ImgSrc, redirectStatus)
             : checkSource(operativeDirective(m_imgSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowStyleFromSource(
    const KURL& url,
    const String& nonce,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  if (isMatchingNoncePresent(operativeDirective(m_styleSrc.get()), nonce))
    return true;
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_styleSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::StyleSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_styleSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFontFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_fontSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::FontSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_fontSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowMediaFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_mediaSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::MediaSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_mediaSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowManifestFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_manifestSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::ManifestSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_manifestSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowConnectToSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   operativeDirective(m_connectSrc.get()), url,
                   ContentSecurityPolicy::DirectiveType::ConnectSrc,
                   redirectStatus)
             : checkSource(operativeDirective(m_connectSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFormAction(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   m_formAction.get(), url,
                   ContentSecurityPolicy::DirectiveType::FormAction,
                   redirectStatus)
             : checkSource(m_formAction.get(), url, redirectStatus);
}

bool CSPDirectiveList::allowBaseURI(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  bool result =
      reportingPolicy == SecurityViolationReportingPolicy::Report
          ? checkSourceAndReportViolation(
                m_baseURI.get(), url,
                ContentSecurityPolicy::DirectiveType::BaseURI, redirectStatus)
          : checkSource(m_baseURI.get(), url, redirectStatus);

  if (result &&
      !checkSource(operativeDirective(m_baseURI.get()), url, redirectStatus)) {
    UseCounter::count(m_policy->document(),
                      UseCounter::BaseWouldBeBlockedByDefaultSrc);
  }

  return result;
}

bool CSPDirectiveList::allowWorkerFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    SecurityViolationReportingPolicy reportingPolicy) const {
  // 'worker-src' overrides 'child-src', which overrides the default
  // sources. So, we do this nested set of calls to 'operativeDirective()' to
  // grab 'worker-src' if it exists, 'child-src' if it doesn't, and 'defaut-src'
  // if neither are available.
  SourceListDirective* whichDirective = operativeDirective(
      m_workerSrc.get(), operativeDirective(m_childSrc.get()));

  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkSourceAndReportViolation(
                   whichDirective, url,
                   ContentSecurityPolicy::DirectiveType::WorkerSrc,
                   redirectStatus)
             : checkSource(whichDirective, url, redirectStatus);
}

bool CSPDirectiveList::allowAncestors(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reportingPolicy) const {
  return reportingPolicy == SecurityViolationReportingPolicy::Report
             ? checkAncestorsAndReportViolation(m_frameAncestors.get(), frame,
                                                url)
             : checkAncestors(m_frameAncestors.get(), frame);
}

bool CSPDirectiveList::allowScriptHash(
    const CSPHashValue& hashValue,
    ContentSecurityPolicy::InlineType type) const {
  if (type == ContentSecurityPolicy::InlineType::Attribute) {
    if (!m_policy->experimentalFeaturesEnabled())
      return false;
    if (!checkHashedAttributes(operativeDirective(m_scriptSrc.get())))
      return false;
  }
  return checkHash(operativeDirective(m_scriptSrc.get()), hashValue);
}

bool CSPDirectiveList::allowStyleHash(
    const CSPHashValue& hashValue,
    ContentSecurityPolicy::InlineType type) const {
  if (type != ContentSecurityPolicy::InlineType::Block)
    return false;
  return checkHash(operativeDirective(m_styleSrc.get()), hashValue);
}

bool CSPDirectiveList::allowDynamic() const {
  return checkDynamic(operativeDirective(m_scriptSrc.get()));
}

const String& CSPDirectiveList::pluginTypesText() const {
  ASSERT(hasPluginTypes());
  return m_pluginTypes->text();
}

bool CSPDirectiveList::shouldSendCSPHeader(Resource::Type type) const {
  // TODO(mkwst): Revisit this once the CORS prefetch issue with the 'CSP'
  //              header is worked out, one way or another:
  //              https://github.com/whatwg/fetch/issues/52
  return false;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::parse(const UChar* begin, const UChar* end) {
  m_header = String(begin, end - begin).stripWhiteSpace();

  if (begin == end)
    return;

  const UChar* position = begin;
  while (position < end) {
    const UChar* directiveBegin = position;
    skipUntil<UChar>(position, end, ';');

    String name, value;
    if (parseDirective(directiveBegin, position, name, value)) {
      ASSERT(!name.isEmpty());
      addDirective(name, value);
    }

    ASSERT(position == end || *position == ';');
    skipExactly<UChar>(position, end, ';');
  }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool CSPDirectiveList::parseDirective(const UChar* begin,
                                      const UChar* end,
                                      String& name,
                                      String& value) {
  ASSERT(name.isEmpty());
  ASSERT(value.isEmpty());

  const UChar* position = begin;
  skipWhile<UChar, isASCIISpace>(position, end);

  // Empty directive (e.g. ";;;"). Exit early.
  if (position == end)
    return false;

  const UChar* nameBegin = position;
  skipWhile<UChar, isCSPDirectiveNameCharacter>(position, end);

  // The directive-name must be non-empty.
  if (nameBegin == position) {
    skipWhile<UChar, isNotASCIISpace>(position, end);
    m_policy->reportUnsupportedDirective(
        String(nameBegin, position - nameBegin));
    return false;
  }

  name = String(nameBegin, position - nameBegin);

  if (position == end)
    return true;

  if (!skipExactly<UChar, isASCIISpace>(position, end)) {
    skipWhile<UChar, isNotASCIISpace>(position, end);
    m_policy->reportUnsupportedDirective(
        String(nameBegin, position - nameBegin));
    return false;
  }

  skipWhile<UChar, isASCIISpace>(position, end);

  const UChar* valueBegin = position;
  skipWhile<UChar, isCSPDirectiveValueCharacter>(position, end);

  if (position != end) {
    m_policy->reportInvalidDirectiveValueCharacter(
        name, String(valueBegin, end - valueBegin));
    return false;
  }

  // The directive-value may be empty.
  if (valueBegin == position)
    return true;

  value = String(valueBegin, position - valueBegin);
  return true;
}

void CSPDirectiveList::parseRequireSRIFor(const String& name,
                                          const String& value) {
  if (m_requireSRIFor != 0) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  StringBuilder tokenErrors;
  unsigned numberOfTokenErrors = 0;
  Vector<UChar> characters;
  value.appendTo(characters);

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  while (position < end) {
    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* tokenBegin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    if (tokenBegin < position) {
      String token = String(tokenBegin, position - tokenBegin);
      if (equalIgnoringCase(token, "script")) {
        m_requireSRIFor |= RequireSRIForToken::Script;
      } else if (equalIgnoringCase(token, "style")) {
        m_requireSRIFor |= RequireSRIForToken::Style;
      } else {
        if (numberOfTokenErrors)
          tokenErrors.append(", \'");
        else
          tokenErrors.append('\'');
        tokenErrors.append(token);
        tokenErrors.append('\'');
        numberOfTokenErrors++;
      }
    }
  }

  if (numberOfTokenErrors == 0)
    return;

  String invalidTokensErrorMessage;
  if (numberOfTokenErrors > 1)
    tokenErrors.append(" are invalid 'require-sri-for' tokens.");
  else
    tokenErrors.append(" is an invalid 'require-sri-for' token.");

  invalidTokensErrorMessage = tokenErrors.toString();

  DCHECK(!invalidTokensErrorMessage.isEmpty());

  m_policy->reportInvalidRequireSRIForTokens(invalidTokensErrorMessage);
}

void CSPDirectiveList::parseReportURI(const String& name, const String& value) {
  if (!m_reportEndpoints.isEmpty()) {
    m_policy->reportDuplicateDirective(name);
    return;
  }

  // Remove report-uri in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta) {
    m_policy->reportInvalidDirectiveInMeta(name);
    return;
  }

  Vector<UChar> characters;
  value.appendTo(characters);

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  while (position < end) {
    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* urlBegin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    if (urlBegin < position) {
      String url = String(urlBegin, position - urlBegin);
      m_reportEndpoints.push_back(url);
    }
  }
}

template <class CSPDirectiveType>
void CSPDirectiveList::setCSPDirective(const String& name,
                                       const String& value,
                                       Member<CSPDirectiveType>& directive) {
  if (directive) {
    m_policy->reportDuplicateDirective(name);
    return;
  }

  // Remove frame-ancestors directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta &&
      ContentSecurityPolicy::getDirectiveType(name) ==
          ContentSecurityPolicy::DirectiveType::FrameAncestors) {
    m_policy->reportInvalidDirectiveInMeta(name);
    return;
  }

  directive = new CSPDirectiveType(name, value, m_policy);
}

void CSPDirectiveList::applySandboxPolicy(const String& name,
                                          const String& sandboxPolicy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta) {
    m_policy->reportInvalidDirectiveInMeta(name);
    return;
  }
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_hasSandboxPolicy) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_hasSandboxPolicy = true;
  String invalidTokens;
  SpaceSplitString policyTokens(AtomicString(sandboxPolicy),
                                SpaceSplitString::ShouldNotFoldCase);
  m_policy->enforceSandboxFlags(
      parseSandboxPolicy(policyTokens, invalidTokens));
  if (!invalidTokens.isNull())
    m_policy->reportInvalidSandboxFlags(invalidTokens);
}

void CSPDirectiveList::treatAsPublicAddress(const String& name,
                                            const String& value) {
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_treatAsPublicAddress) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_treatAsPublicAddress = true;
  m_policy->treatAsPublicAddress();
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::enforceStrictMixedContentChecking(const String& name,
                                                         const String& value) {
  if (m_strictMixedContentCheckingEnforced) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);

  m_strictMixedContentCheckingEnforced = true;

  if (!isReportOnly())
    m_policy->enforceStrictMixedContentChecking();
}

void CSPDirectiveList::enableInsecureRequestsUpgrade(const String& name,
                                                     const String& value) {
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_upgradeInsecureRequests) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_upgradeInsecureRequests = true;

  m_policy->upgradeInsecureRequests();
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::addDirective(const String& name, const String& value) {
  ASSERT(!name.isEmpty());

  ContentSecurityPolicy::DirectiveType type =
      ContentSecurityPolicy::getDirectiveType(name);
  if (type == ContentSecurityPolicy::DirectiveType::DefaultSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_defaultSrc);
    // TODO(mkwst) It seems unlikely that developers would use different
    // algorithms for scripts and styles. We may want to combine the
    // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
    m_policy->usesScriptHashAlgorithms(m_defaultSrc->hashAlgorithmsUsed());
    m_policy->usesStyleHashAlgorithms(m_defaultSrc->hashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::ScriptSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_scriptSrc);
    m_policy->usesScriptHashAlgorithms(m_scriptSrc->hashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::ObjectSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_objectSrc);
  } else if (type ==

             ContentSecurityPolicy::DirectiveType::FrameAncestors) {
    setCSPDirective<SourceListDirective>(name, value, m_frameAncestors);
  } else if (type == ContentSecurityPolicy::DirectiveType::FrameSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_frameSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::ImgSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_imgSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::StyleSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_styleSrc);
    m_policy->usesStyleHashAlgorithms(m_styleSrc->hashAlgorithmsUsed());
  } else if (type == ContentSecurityPolicy::DirectiveType::FontSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_fontSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::MediaSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_mediaSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::ConnectSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_connectSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::Sandbox) {
    applySandboxPolicy(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::ReportURI) {
    parseReportURI(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::BaseURI) {
    setCSPDirective<SourceListDirective>(name, value, m_baseURI);
  } else if (type == ContentSecurityPolicy::DirectiveType::ChildSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_childSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::WorkerSrc &&
             m_policy->experimentalFeaturesEnabled()) {
    setCSPDirective<SourceListDirective>(name, value, m_workerSrc);
  } else if (type == ContentSecurityPolicy::DirectiveType::FormAction) {
    setCSPDirective<SourceListDirective>(name, value, m_formAction);
  } else if (type == ContentSecurityPolicy::DirectiveType::PluginTypes) {
    setCSPDirective<MediaListDirective>(name, value, m_pluginTypes);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::UpgradeInsecureRequests) {
    enableInsecureRequestsUpgrade(name, value);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::BlockAllMixedContent) {
    enforceStrictMixedContentChecking(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::ManifestSrc) {
    setCSPDirective<SourceListDirective>(name, value, m_manifestSrc);
  } else if (type ==
             ContentSecurityPolicy::DirectiveType::TreatAsPublicAddress) {
    treatAsPublicAddress(name, value);
  } else if (type == ContentSecurityPolicy::DirectiveType::RequireSRIFor &&
             m_policy->experimentalFeaturesEnabled()) {
    parseRequireSRIFor(name, value);
  } else {
    m_policy->reportUnsupportedDirective(name);
  }
}

SourceListDirective* CSPDirectiveList::operativeDirective(
    const ContentSecurityPolicy::DirectiveType& type) const {
  switch (type) {
    // Directives that do not have a default directive.
    case ContentSecurityPolicy::DirectiveType::BaseURI:
      return m_baseURI.get();
    case ContentSecurityPolicy::DirectiveType::DefaultSrc:
      return m_defaultSrc.get();
    case ContentSecurityPolicy::DirectiveType::FrameAncestors:
      return m_frameAncestors.get();
    case ContentSecurityPolicy::DirectiveType::FormAction:
      return m_formAction.get();
    // Directives that have one default directive.
    case ContentSecurityPolicy::DirectiveType::ChildSrc:
      return operativeDirective(m_childSrc.get());
    case ContentSecurityPolicy::DirectiveType::ConnectSrc:
      return operativeDirective(m_connectSrc.get());
    case ContentSecurityPolicy::DirectiveType::FontSrc:
      return operativeDirective(m_fontSrc.get());
    case ContentSecurityPolicy::DirectiveType::ImgSrc:
      return operativeDirective(m_imgSrc.get());
    case ContentSecurityPolicy::DirectiveType::ManifestSrc:
      return operativeDirective(m_manifestSrc.get());
    case ContentSecurityPolicy::DirectiveType::MediaSrc:
      return operativeDirective(m_mediaSrc.get());
    case ContentSecurityPolicy::DirectiveType::ObjectSrc:
      return operativeDirective(m_objectSrc.get());
    case ContentSecurityPolicy::DirectiveType::ScriptSrc:
      return operativeDirective(m_scriptSrc.get());
    case ContentSecurityPolicy::DirectiveType::StyleSrc:
      return operativeDirective(m_styleSrc.get());
    // Directives that default to child-src, which defaults to default-src.
    case ContentSecurityPolicy::DirectiveType::FrameSrc:
      return operativeDirective(m_frameSrc.get(),
                                operativeDirective(m_childSrc.get()));
    // TODO(mkwst): Reevaluate this.
    case ContentSecurityPolicy::DirectiveType::WorkerSrc:
      return operativeDirective(m_workerSrc.get(),
                                operativeDirective(m_childSrc.get()));
    default:
      return nullptr;
  }
}

SourceListDirectiveVector CSPDirectiveList::getSourceVector(
    const ContentSecurityPolicy::DirectiveType& type,
    const CSPDirectiveListVector& policies) {
  SourceListDirectiveVector sourceListDirectives;
  for (const auto& policy : policies) {
    if (SourceListDirective* directive = policy->operativeDirective(type)) {
      if (directive->isNone())
        return SourceListDirectiveVector(1, directive);
      sourceListDirectives.push_back(directive);
    }
  }

  return sourceListDirectives;
}

bool CSPDirectiveList::subsumes(const CSPDirectiveListVector& other) {
  // A white-list of directives that we consider for subsumption.
  // See more about source lists here:
  // https://w3c.github.io/webappsec-csp/#framework-directive-source-list
  ContentSecurityPolicy::DirectiveType directives[] = {
      ContentSecurityPolicy::DirectiveType::ChildSrc,
      ContentSecurityPolicy::DirectiveType::ConnectSrc,
      ContentSecurityPolicy::DirectiveType::FontSrc,
      ContentSecurityPolicy::DirectiveType::FrameSrc,
      ContentSecurityPolicy::DirectiveType::ImgSrc,
      ContentSecurityPolicy::DirectiveType::ManifestSrc,
      ContentSecurityPolicy::DirectiveType::MediaSrc,
      ContentSecurityPolicy::DirectiveType::ObjectSrc,
      ContentSecurityPolicy::DirectiveType::ScriptSrc,
      ContentSecurityPolicy::DirectiveType::StyleSrc,
      ContentSecurityPolicy::DirectiveType::WorkerSrc,
      ContentSecurityPolicy::DirectiveType::BaseURI,
      ContentSecurityPolicy::DirectiveType::FrameAncestors,
      ContentSecurityPolicy::DirectiveType::FormAction};

  for (const auto& directive : directives) {
    // There should only be one SourceListDirective for each directive in
    // Embedding-CSP.
    SourceListDirectiveVector requiredList =
        getSourceVector(directive, CSPDirectiveListVector(1, this));
    if (!requiredList.size())
      continue;
    SourceListDirective* required = requiredList[0];
    // Aggregate all serialized source lists of the returned CSP into a vector
    // based on a directive type, defaulting accordingly (for example, to
    // `default-src`).
    SourceListDirectiveVector returned = getSourceVector(directive, other);
    // TODO(amalika): Add checks for plugin-types, sandbox, disown-opener,
    // navigation-to, worker-src.
    if (!required->subsumes(returned))
      return false;
  }

  if (!hasPluginTypes())
    return true;

  HeapVector<Member<MediaListDirective>> pluginTypesOther;
  for (const auto& policy : other) {
    if (policy->hasPluginTypes())
      pluginTypesOther.push_back(policy->m_pluginTypes);
  }

  return m_pluginTypes->subsumes(pluginTypesOther);
}

WebContentSecurityPolicyPolicy CSPDirectiveList::exposeForNavigationalChecks()
    const {
  WebContentSecurityPolicyPolicy policy;
  policy.disposition = static_cast<WebContentSecurityPolicyType>(m_headerType);
  policy.source = static_cast<WebContentSecurityPolicySource>(m_headerSource);
  std::vector<WebContentSecurityPolicyDirective> directives;
  for (const auto& directive :
       {m_childSrc, m_defaultSrc, m_formAction, m_frameSrc}) {
    if (directive) {
      directives.push_back(WebContentSecurityPolicyDirective{
          directive->directiveName(),
          directive->exposeForNavigationalChecks()});
    }
  }
  policy.directives = directives;
  policy.reportEndpoints = reportEndpoints();
  policy.header = header();

  return policy;
}

DEFINE_TRACE(CSPDirectiveList) {
  visitor->trace(m_policy);
  visitor->trace(m_pluginTypes);
  visitor->trace(m_baseURI);
  visitor->trace(m_childSrc);
  visitor->trace(m_connectSrc);
  visitor->trace(m_defaultSrc);
  visitor->trace(m_fontSrc);
  visitor->trace(m_formAction);
  visitor->trace(m_frameAncestors);
  visitor->trace(m_frameSrc);
  visitor->trace(m_imgSrc);
  visitor->trace(m_mediaSrc);
  visitor->trace(m_manifestSrc);
  visitor->trace(m_objectSrc);
  visitor->trace(m_scriptSrc);
  visitor->trace(m_styleSrc);
  visitor->trace(m_workerSrc);
}

}  // namespace blink
