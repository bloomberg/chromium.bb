/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContentSecurityPolicy_h
#define ContentSecurityPolicy_h

#include <memory>
#include <utility>
#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebInsecureRequestPolicy.h"

namespace WTF {
class OrdinalNumber;
}

namespace blink {

class ContentSecurityPolicyResponseHeaders;
class ConsoleMessage;
class CSPDirectiveList;
class CSPSource;
class Document;
class Element;
class LocalFrameClient;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class SecurityPolicyViolationEventInit;
class SourceLocation;

typedef int SandboxFlags;
typedef HeapVector<Member<CSPDirectiveList>> CSPDirectiveListVector;
typedef HeapVector<Member<ConsoleMessage>> ConsoleMessageVector;
typedef std::pair<String, ContentSecurityPolicyHeaderType> CSPHeaderAndType;
using RedirectStatus = ResourceRequest::RedirectStatus;

class CORE_EXPORT ContentSecurityPolicy
    : public GarbageCollectedFinalized<ContentSecurityPolicy> {
 public:
  enum ExceptionStatus { kWillThrowException, kWillNotThrowException };

  // This covers the possible values of a violation's 'resource', as defined in
  // https://w3c.github.io/webappsec-csp/#violation-resource. By the time we
  // generate a report, we're guaranteed that the value isn't 'null', so we
  // don't need that state in this enum.
  enum ViolationType { kInlineViolation, kEvalViolation, kURLViolation };

  enum class InlineType { kBlock, kAttribute };

  enum class DirectiveType {
    kUndefined,
    kBaseURI,
    kBlockAllMixedContent,
    kChildSrc,
    kConnectSrc,
    kDefaultSrc,
    kFrameAncestors,
    kFrameSrc,
    kFontSrc,
    kFormAction,
    kImgSrc,
    kManifestSrc,
    kMediaSrc,
    kObjectSrc,
    kPluginTypes,
    kReportURI,
    kRequireSRIFor,
    kSandbox,
    kScriptSrc,
    kStyleSrc,
    kTreatAsPublicAddress,
    kUpgradeInsecureRequests,
    kWorkerSrc,
  };

  // CheckHeaderType can be passed to Allow*FromSource methods to control which
  // types of CSP headers are checked.
  enum class CheckHeaderType {
    // Check both Content-Security-Policy and
    // Content-Security-Policy-Report-Only headers.
    kCheckAll,
    // Check Content-Security-Policy headers only and ignore
    // Content-Security-Policy-Report-Only headers.
    kCheckEnforce,
    // Check Content-Security-Policy-Report-Only headers only and ignore
    // Content-Security-Policy headers.
    kCheckReportOnly
  };

  static const size_t kMaxSampleLength = 40;

  static ContentSecurityPolicy* Create() { return new ContentSecurityPolicy(); }
  ~ContentSecurityPolicy();
  DECLARE_TRACE();

  void BindToExecutionContext(ExecutionContext*);
  void SetupSelf(const SecurityOrigin&);
  void CopyStateFrom(const ContentSecurityPolicy*);
  void CopyPluginTypesFrom(const ContentSecurityPolicy*);

  void DidReceiveHeaders(const ContentSecurityPolicyResponseHeaders&);
  void DidReceiveHeader(const String&,
                        ContentSecurityPolicyHeaderType,
                        ContentSecurityPolicyHeaderSource);
  void AddPolicyFromHeaderValue(const String&,
                                ContentSecurityPolicyHeaderType,
                                ContentSecurityPolicyHeaderSource);
  void ReportAccumulatedHeaders(LocalFrameClient*) const;

  std::unique_ptr<Vector<CSPHeaderAndType>> Headers() const;

  // |element| will not be present for navigations to javascript URLs,
  // as those checks happen in the middle of the navigation algorithm,
  // and we generally don't have access to the responsible element.
  bool AllowJavaScriptURLs(Element*,
                           const String& source,
                           const String& context_url,
                           const WTF::OrdinalNumber& context_line,
                           SecurityViolationReportingPolicy =
                               SecurityViolationReportingPolicy::kReport) const;

  // |element| will be present almost all of the time, but because of
  // strangeness around targeting handlers for '<body>', '<svg>', and
  // '<frameset>', it will be 'nullptr' for handlers on those
  // elements.
  bool AllowInlineEventHandler(
      Element*,
      const String& source,
      const String& context_url,
      const WTF::OrdinalNumber& context_line,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport) const;
  // When the reporting status is |SendReport|, the |ExceptionStatus|
  // should indicate whether the caller will throw a JavaScript
  // exception in the event of a violation. When the caller will throw
  // an exception, ContentSecurityPolicy does not log a violation
  // message to the console because it would be redundant.
  bool AllowEval(ScriptState*,
                 SecurityViolationReportingPolicy,
                 ExceptionStatus,
                 const String& script_content) const;
  bool AllowPluginType(const String& type,
                       const String& type_attribute,
                       const KURL&,
                       SecurityViolationReportingPolicy =
                           SecurityViolationReportingPolicy::kReport) const;
  // Checks whether the plugin type should be allowed in the given
  // document; enforces the CSP rule that PluginDocuments inherit
  // plugin-types directives from the parent document.
  bool AllowPluginTypeForDocument(
      const Document&,
      const String& type,
      const String& type_attribute,
      const KURL&,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport) const;

  bool AllowObjectFromSource(
      const KURL&,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowFrameFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowImageFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowFontFromSource(const KURL&,
                           RedirectStatus = RedirectStatus::kNoRedirect,
                           SecurityViolationReportingPolicy =
                               SecurityViolationReportingPolicy::kReport,
                           CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowMediaFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowConnectToSource(const KURL&,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowFormAction(const KURL&,
                       RedirectStatus = RedirectStatus::kNoRedirect,
                       SecurityViolationReportingPolicy =
                           SecurityViolationReportingPolicy::kReport,
                       CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowBaseURI(const KURL&,
                    RedirectStatus = RedirectStatus::kNoRedirect,
                    SecurityViolationReportingPolicy =
                        SecurityViolationReportingPolicy::kReport) const;
  bool AllowWorkerContextFromSource(
      const KURL&,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;

  bool AllowManifestFromSource(
      const KURL&,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;

  // Passing 'String()' into the |nonce| arguments in the following methods
  // represents an unnonced resource load.
  bool AllowScriptFromSource(
      const KURL&,
      const String& nonce,
      const IntegrityMetadataSet& hashes,
      ParserDisposition,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowStyleFromSource(const KURL&,
                            const String& nonce,
                            RedirectStatus = RedirectStatus::kNoRedirect,
                            SecurityViolationReportingPolicy =
                                SecurityViolationReportingPolicy::kReport,
                            CheckHeaderType = CheckHeaderType::kCheckAll) const;
  bool AllowInlineScript(Element*,
                         const String& context_url,
                         const String& nonce,
                         const WTF::OrdinalNumber& context_line,
                         const String& script_content,
                         SecurityViolationReportingPolicy =
                             SecurityViolationReportingPolicy::kReport) const;
  bool AllowInlineStyle(Element*,
                        const String& context_url,
                        const String& nonce,
                        const WTF::OrdinalNumber& context_line,
                        const String& style_content,
                        SecurityViolationReportingPolicy =
                            SecurityViolationReportingPolicy::kReport) const;

  // |allowAncestors| does not need to know whether the resource was a
  // result of a redirect. After a redirect, source paths are usually
  // ignored to stop a page from learning the path to which the
  // request was redirected, but this is not a concern for ancestors,
  // because a child frame can't manipulate the URL of a cross-origin
  // parent.
  bool AllowAncestors(LocalFrame*,
                      const KURL&,
                      SecurityViolationReportingPolicy =
                          SecurityViolationReportingPolicy::kReport) const;
  bool IsFrameAncestorsEnforced() const;

  // The hash allow functions are guaranteed to not have any side
  // effects, including reporting.
  // Hash functions check all policies relating to use of a script/style
  // with the given hash and return true all CSP policies allow it.
  // If these return true, callers can then process the content or
  // issue a load and be safe disabling any further CSP checks.
  //
  // TODO(mkwst): Fold hashes into 'allow{Script,Style}' checks above, just
  // as we've done with nonces. https://crbug.com/617065
  bool AllowScriptWithHash(const String& source, InlineType) const;
  bool AllowStyleWithHash(const String& source, InlineType) const;

  bool AllowRequestWithoutIntegrity(
      WebURLRequest::RequestContext,
      const KURL&,
      RedirectStatus = RedirectStatus::kNoRedirect,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::kReport,
      CheckHeaderType = CheckHeaderType::kCheckAll) const;

  bool AllowRequest(WebURLRequest::RequestContext,
                    const KURL&,
                    const String& nonce,
                    const IntegrityMetadataSet&,
                    ParserDisposition,
                    RedirectStatus = RedirectStatus::kNoRedirect,
                    SecurityViolationReportingPolicy =
                        SecurityViolationReportingPolicy::kReport,
                    CheckHeaderType = CheckHeaderType::kCheckAll) const;

  void UsesScriptHashAlgorithms(uint8_t content_security_policy_hash_algorithm);
  void UsesStyleHashAlgorithms(uint8_t content_security_policy_hash_algorithm);

  void SetOverrideAllowInlineStyle(bool);
  void SetOverrideURLForSelf(const KURL&);

  bool IsActive() const;

  // If a frame is passed in, the message will be logged to its active
  // document's console.  Otherwise, the message will be logged to this object's
  // |m_executionContext|.
  void LogToConsole(ConsoleMessage*, LocalFrame* = nullptr);

  void ReportDirectiveAsSourceExpression(const String& directive_name,
                                         const String& source_expression);
  void ReportDuplicateDirective(const String&);
  void ReportInvalidDirectiveValueCharacter(const String& directive_name,
                                            const String& value);
  void ReportInvalidPathCharacter(const String& directive_name,
                                  const String& value,
                                  const char);
  void ReportInvalidPluginTypes(const String&);
  void ReportInvalidRequireSRIForTokens(const String&);
  void ReportInvalidSandboxFlags(const String&);
  void ReportInvalidSourceExpression(const String& directive_name,
                                     const String& source);
  void ReportMissingReportURI(const String&);
  void ReportUnsupportedDirective(const String&);
  void ReportInvalidInReportOnly(const String&);
  void ReportInvalidDirectiveInMeta(const String& directive_name);
  void ReportReportOnlyInMeta(const String&);
  void ReportMetaOutsideHead(const String&);
  void ReportValueForEmptyDirective(const String& directive_name,
                                    const String& value);

  // If a frame is passed in, the report will be sent using it as a context. If
  // no frame is passed in, the report will be sent via this object's
  // |m_executionContext| (or dropped on the floor if no such context is
  // available).
  // If |sourceLocation| is not set, the source location will be the context's
  // current location.
  void ReportViolation(const String& directive_text,
                       const DirectiveType& effective_type,
                       const String& console_message,
                       const KURL& blocked_url,
                       const Vector<String>& report_endpoints,
                       const String& header,
                       ContentSecurityPolicyHeaderType,
                       ViolationType,
                       std::unique_ptr<SourceLocation>,
                       LocalFrame* = nullptr,
                       RedirectStatus = RedirectStatus::kFollowedRedirect,
                       Element* = nullptr,
                       const String& source = g_empty_string);

  // Called when mixed content is detected on a page; will trigger a violation
  // report if the 'block-all-mixed-content' directive is specified for a
  // policy.
  void ReportMixedContent(const KURL& mixed_url, RedirectStatus);

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) const;

  const KURL Url() const;
  void EnforceSandboxFlags(SandboxFlags);
  void TreatAsPublicAddress();
  String EvalDisabledErrorMessage() const;

  // Upgrade-Insecure-Requests and Block-All-Mixed-Content are represented in
  // |m_insecureRequestPolicy|
  void EnforceStrictMixedContentChecking();
  void UpgradeInsecureRequests();
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const {
    return insecure_request_policy_;
  }

  bool UrlMatchesSelf(const KURL&) const;
  bool ProtocolEqualsSelf(const String&) const;
  const String& GetSelfProtocol() const;

  bool ExperimentalFeaturesEnabled() const;

  bool ShouldSendCSPHeader(Resource::Type) const;

  CSPSource* GetSelfSource() const { return self_source_; }

  static bool ShouldBypassMainWorld(const ExecutionContext*);
  static bool ShouldBypassContentSecurityPolicy(
      const KURL&,
      SchemeRegistry::PolicyAreas = SchemeRegistry::kPolicyAreaAll);

  static bool IsNonceableElement(const Element*);

  // This method checks whether the request should be allowed for an
  // experimental EmbeddingCSP feature
  // Please, see https://w3c.github.io/webappsec-csp/embedded/#origin-allowed.
  static bool ShouldEnforceEmbeddersPolicy(const ResourceResponse&,
                                           SecurityOrigin*);

  static const char* GetDirectiveName(const DirectiveType&);
  static DirectiveType GetDirectiveType(const String& name);

  // This method checks if if this policy subsumes a given policy.
  // Note the correct result is guaranteed if this policy contains only one
  // CSPDirectiveList. More information here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool Subsumes(const ContentSecurityPolicy&) const;

  Document* GetDocument() const;

  bool HasHeaderDeliveredPolicy() const { return header_delivered_; }

  static bool IsValidCSPAttr(const String& attr);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceInline);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceSinglePolicy);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceMultiplePolicy);
  FRIEND_TEST_ALL_PREFIXES(BaseFetchContextTest,
                           RedirectChecksReportedAndEnforcedCSP);
  FRIEND_TEST_ALL_PREFIXES(BaseFetchContextTest,
                           AllowResponseChecksReportedAndEnforcedCSP);
  FRIEND_TEST_ALL_PREFIXES(FrameFetchContextTest,
                           PopulateResourceRequestChecksReportOnlyCSP);

  ContentSecurityPolicy();

  void ApplyPolicySideEffectsToExecutionContext();

  KURL CompleteURL(const String&) const;

  void LogToConsole(const String& message, MessageLevel = kErrorMessageLevel);

  void AddAndReportPolicyFromHeaderValue(const String&,
                                         ContentSecurityPolicyHeaderType,
                                         ContentSecurityPolicyHeaderSource);

  bool ShouldSendViolationReport(const String&) const;
  void DidSendViolationReport(const String&);
  void DispatchViolationEvents(const SecurityPolicyViolationEventInit&,
                               Element*);
  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           LocalFrame*,
                           const Vector<String>& report_endpoints);

  Member<ExecutionContext> execution_context_;
  bool override_inline_style_allowed_;
  CSPDirectiveListVector policies_;
  ConsoleMessageVector console_messages_;
  bool header_delivered_{false};

  HashSet<unsigned, AlreadyHashed> violation_reports_sent_;

  // We put the hash functions used on the policy object so that we only need
  // to calculate a hash once and then distribute it to all of the directives
  // for validation.
  uint8_t script_hash_algorithms_used_;
  uint8_t style_hash_algorithms_used_;

  // State flags used to configure the environment after parsing a policy.
  SandboxFlags sandbox_mask_;
  bool treat_as_public_address_;
  String disable_eval_error_message_;
  WebInsecureRequestPolicy insecure_request_policy_;

  Member<CSPSource> self_source_;
  String self_protocol_;
};

}  // namespace blink

#endif
