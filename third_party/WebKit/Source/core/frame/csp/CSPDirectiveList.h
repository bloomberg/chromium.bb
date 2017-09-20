// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPDirectiveList_h
#define CSPDirectiveList_h

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/frame/csp/MediaListDirective.h"
#include "core/frame/csp/SourceListDirective.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebContentSecurityPolicy.h"

namespace blink {

class ContentSecurityPolicy;

typedef HeapVector<Member<SourceListDirective>> SourceListDirectiveVector;

class CORE_EXPORT CSPDirectiveList
    : public GarbageCollectedFinalized<CSPDirectiveList> {
  WTF_MAKE_NONCOPYABLE(CSPDirectiveList);

 public:
  static CSPDirectiveList* Create(ContentSecurityPolicy*,
                                  const UChar* begin,
                                  const UChar* end,
                                  ContentSecurityPolicyHeaderType,
                                  ContentSecurityPolicyHeaderSource);

  void Parse(const UChar* begin, const UChar* end);

  const String& Header() const { return header_; }
  ContentSecurityPolicyHeaderType HeaderType() const { return header_type_; }
  ContentSecurityPolicyHeaderSource HeaderSource() const {
    return header_source_;
  }

  bool AllowJavaScriptURLs(Element*,
                           const String& source,
                           const String& context_url,
                           const WTF::OrdinalNumber& context_line,
                           SecurityViolationReportingPolicy) const;
  bool AllowInlineEventHandlers(Element*,
                                const String& source,
                                const String& context_url,
                                const WTF::OrdinalNumber& context_line,
                                SecurityViolationReportingPolicy) const;
  bool AllowInlineScript(Element*,
                         const String& context_url,
                         const String& nonce,
                         const WTF::OrdinalNumber& context_line,
                         SecurityViolationReportingPolicy,
                         const String& script_content) const;
  bool AllowInlineStyle(Element*,
                        const String& context_url,
                        const String& nonce,
                        const WTF::OrdinalNumber& context_line,
                        SecurityViolationReportingPolicy,
                        const String& style_content) const;
  bool AllowEval(ScriptState*,
                 SecurityViolationReportingPolicy,
                 ContentSecurityPolicy::ExceptionStatus,
                 const String& script_content) const;
  bool AllowPluginType(const String& type,
                       const String& type_attribute,
                       const KURL&,
                       SecurityViolationReportingPolicy) const;

  bool AllowScriptFromSource(const KURL&,
                             const String& nonce,
                             const IntegrityMetadataSet& hashes,
                             ParserDisposition,
                             ResourceRequest::RedirectStatus,
                             SecurityViolationReportingPolicy) const;
  bool AllowStyleFromSource(const KURL&,
                            const String& nonce,
                            ResourceRequest::RedirectStatus,
                            SecurityViolationReportingPolicy) const;

  bool AllowObjectFromSource(const KURL&,
                             ResourceRequest::RedirectStatus,
                             SecurityViolationReportingPolicy) const;
  bool AllowFrameFromSource(const KURL&,
                            ResourceRequest::RedirectStatus,
                            SecurityViolationReportingPolicy) const;
  bool AllowImageFromSource(const KURL&,
                            ResourceRequest::RedirectStatus,
                            SecurityViolationReportingPolicy) const;
  bool AllowFontFromSource(const KURL&,
                           ResourceRequest::RedirectStatus,
                           SecurityViolationReportingPolicy) const;
  bool AllowMediaFromSource(const KURL&,
                            ResourceRequest::RedirectStatus,
                            SecurityViolationReportingPolicy) const;
  bool AllowManifestFromSource(const KURL&,
                               ResourceRequest::RedirectStatus,
                               SecurityViolationReportingPolicy) const;
  bool AllowConnectToSource(const KURL&,
                            ResourceRequest::RedirectStatus,
                            SecurityViolationReportingPolicy) const;
  bool AllowFormAction(const KURL&,
                       ResourceRequest::RedirectStatus,
                       SecurityViolationReportingPolicy) const;
  bool AllowBaseURI(const KURL&,
                    ResourceRequest::RedirectStatus,
                    SecurityViolationReportingPolicy) const;
  bool AllowWorkerFromSource(const KURL&,
                             ResourceRequest::RedirectStatus,
                             SecurityViolationReportingPolicy) const;
  // |allowAncestors| does not need to know whether the resource was a
  // result of a redirect. After a redirect, source paths are usually
  // ignored to stop a page from learning the path to which the
  // request was redirected, but this is not a concern for ancestors,
  // because a child frame can't manipulate the URL of a cross-origin
  // parent.
  bool AllowAncestors(LocalFrame*,
                      const KURL&,
                      SecurityViolationReportingPolicy) const;
  bool AllowScriptHash(const CSPHashValue&,
                       ContentSecurityPolicy::InlineType) const;
  bool AllowStyleHash(const CSPHashValue&,
                      ContentSecurityPolicy::InlineType) const;
  bool AllowDynamic() const;
  bool AllowDynamicWorker() const;

  bool AllowRequestWithoutIntegrity(WebURLRequest::RequestContext,
                                    const KURL&,
                                    ResourceRequest::RedirectStatus,
                                    SecurityViolationReportingPolicy) const;

  bool StrictMixedContentChecking() const {
    return strict_mixed_content_checking_enforced_;
  }
  void ReportMixedContent(const KURL& mixed_url,
                          ResourceRequest::RedirectStatus) const;

  const String& EvalDisabledErrorMessage() const {
    return eval_disabled_error_message_;
  }
  bool IsReportOnly() const {
    return header_type_ == kContentSecurityPolicyHeaderTypeReport;
  }
  const Vector<String>& ReportEndpoints() const { return report_endpoints_; }
  bool UseReportingApi() const { return use_reporting_api_; }
  uint8_t RequireSRIForTokens() const { return require_sri_for_; }
  bool IsFrameAncestorsEnforced() const {
    return frame_ancestors_.Get() && !IsReportOnly();
  }

  // Used to copy plugin-types into a plugin document in a nested
  // browsing context.
  bool HasPluginTypes() const { return !!plugin_types_; }
  const String& PluginTypesText() const;

  bool ShouldSendCSPHeader(Resource::Type) const;

  // The algorithm is described here:
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool Subsumes(const CSPDirectiveListVector&);

  // Export a subset of the Policy. The primary goal of this method is to make
  // the embedders aware of the directives that affect navigation, as the
  // embedder is responsible for navigational enforcement.
  // It currently contains the following ones:
  // * default-src
  // * child-src
  // * frame-src
  // * form-action
  // * upgrade-insecure-requests
  // The exported directives only contains sources that affect navigation. For
  // instance it doesn't contains 'unsafe-inline' or 'unsafe-eval'
  WebContentSecurityPolicy ExposeForNavigationalChecks() const;

  DECLARE_TRACE();

 private:
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, IsMatchingNoncePresent);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, GetSourceVector);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, OperativeDirectiveGivenType);

  enum RequireSRIForToken { kNone = 0, kScript = 1 << 0, kStyle = 1 << 1 };

  CSPDirectiveList(ContentSecurityPolicy*,
                   ContentSecurityPolicyHeaderType,
                   ContentSecurityPolicyHeaderSource);

  bool ParseDirective(const UChar* begin,
                      const UChar* end,
                      String& name,
                      String& value);
  void ParseRequireSRIFor(const String& name, const String& value);
  void ParseReportURI(const String& name, const String& value);
  void ParseReportTo(const String& name, const String& value);
  void ParseAndAppendReportEndpoints(const String& value);
  void ParsePluginTypes(const String& name, const String& value);
  void AddDirective(const String& name, const String& value);
  void ApplySandboxPolicy(const String& name, const String& sandbox_policy);
  void EnforceStrictMixedContentChecking(const String& name,
                                         const String& value);
  void EnableInsecureRequestsUpgrade(const String& name, const String& value);
  void TreatAsPublicAddress(const String& name, const String& value);
  void RequireTrustedTypes(const String& name, const String& value);

  template <class CSPDirectiveType>
  void SetCSPDirective(const String& name,
                       const String& value,
                       Member<CSPDirectiveType>&);

  SourceListDirective* OperativeDirective(SourceListDirective*) const;
  SourceListDirective* OperativeDirective(SourceListDirective*,
                                          SourceListDirective* override) const;
  void ReportViolation(const String& directive_text,
                       const ContentSecurityPolicy::DirectiveType&,
                       const String& console_message,
                       const KURL& blocked_url,
                       ResourceRequest::RedirectStatus) const;
  void ReportViolationWithFrame(const String& directive_text,
                                const ContentSecurityPolicy::DirectiveType&,
                                const String& console_message,
                                const KURL& blocked_url,
                                LocalFrame*) const;
  void ReportViolationWithLocation(const String& directive_text,
                                   const ContentSecurityPolicy::DirectiveType&,
                                   const String& console_message,
                                   const KURL& blocked_url,
                                   const String& context_url,
                                   const WTF::OrdinalNumber& context_line,
                                   Element*,
                                   const String& source) const;
  void ReportEvalViolation(const String& directive_text,
                           const ContentSecurityPolicy::DirectiveType&,
                           const String& message,
                           const KURL& blocked_url,
                           ScriptState*,
                           const ContentSecurityPolicy::ExceptionStatus,
                           const String& content) const;

  bool CheckEval(SourceListDirective*) const;
  bool CheckDynamic(SourceListDirective*) const;
  bool IsMatchingNoncePresent(SourceListDirective*, const String&) const;
  bool AreAllMatchingHashesPresent(SourceListDirective*,
                                   const IntegrityMetadataSet&) const;
  bool CheckHash(SourceListDirective*, const CSPHashValue&) const;
  bool CheckHashedAttributes(SourceListDirective*) const;
  bool CheckSource(SourceListDirective*,
                   const KURL&,
                   ResourceRequest::RedirectStatus) const;
  bool CheckMediaType(MediaListDirective*,
                      const String& type,
                      const String& type_attribute) const;
  bool CheckAncestors(SourceListDirective*, LocalFrame*) const;
  bool CheckRequestWithoutIntegrity(WebURLRequest::RequestContext) const;

  void SetEvalDisabledErrorMessage(const String& error_message) {
    eval_disabled_error_message_ = error_message;
  }

  bool CheckEvalAndReportViolation(SourceListDirective*,
                                   const String& console_message,
                                   ScriptState*,
                                   ContentSecurityPolicy::ExceptionStatus,
                                   const String& script_content) const;
  bool CheckInlineAndReportViolation(SourceListDirective*,
                                     const String& console_message,
                                     Element*,
                                     const String& source,
                                     const String& context_url,
                                     const WTF::OrdinalNumber& context_line,
                                     bool is_script,
                                     const String& hash_value) const;

  bool CheckSourceAndReportViolation(
      SourceListDirective*,
      const KURL&,
      const ContentSecurityPolicy::DirectiveType&,
      ResourceRequest::RedirectStatus) const;
  bool CheckMediaTypeAndReportViolation(MediaListDirective*,
                                        const String& type,
                                        const String& type_attribute,
                                        const String& console_message) const;
  bool CheckAncestorsAndReportViolation(SourceListDirective*,
                                        LocalFrame*,
                                        const KURL&) const;
  bool CheckRequestWithoutIntegrityAndReportViolation(
      WebURLRequest::RequestContext,
      const KURL&,
      ResourceRequest::RedirectStatus) const;

  bool DenyIfEnforcingPolicy() const { return IsReportOnly(); }

  // This function returns a SourceListDirective of a given type
  // or if it is not defined, the default SourceListDirective for that type.
  SourceListDirective* OperativeDirective(
      const ContentSecurityPolicy::DirectiveType&) const;

  // This function aggregates from a vector of policies all operative
  // SourceListDirectives of a given type into a vector.
  static SourceListDirectiveVector GetSourceVector(
      const ContentSecurityPolicy::DirectiveType&,
      const CSPDirectiveListVector& policies);

  Member<ContentSecurityPolicy> policy_;

  String header_;
  ContentSecurityPolicyHeaderType header_type_;
  ContentSecurityPolicyHeaderSource header_source_;

  bool has_sandbox_policy_;

  bool strict_mixed_content_checking_enforced_;

  bool upgrade_insecure_requests_;
  bool treat_as_public_address_;
  bool require_safe_types_;

  Member<MediaListDirective> plugin_types_;
  Member<SourceListDirective> base_uri_;
  Member<SourceListDirective> child_src_;
  Member<SourceListDirective> connect_src_;
  Member<SourceListDirective> default_src_;
  Member<SourceListDirective> font_src_;
  Member<SourceListDirective> form_action_;
  Member<SourceListDirective> frame_ancestors_;
  Member<SourceListDirective> frame_src_;
  Member<SourceListDirective> img_src_;
  Member<SourceListDirective> media_src_;
  Member<SourceListDirective> manifest_src_;
  Member<SourceListDirective> object_src_;
  Member<SourceListDirective> script_src_;
  Member<SourceListDirective> style_src_;
  Member<SourceListDirective> worker_src_;

  uint8_t require_sri_for_;

  Vector<String> report_endpoints_;
  bool use_reporting_api_;

  String eval_disabled_error_message_;
};

}  // namespace blink

#endif
