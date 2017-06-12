/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "platform/weborigin/SchemeRegistry.h"

#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/text/StringBuilder.h"
#include "url/url_util.h"

namespace blink {

namespace {

struct PolicyAreasHashTraits : HashTraits<SchemeRegistry::PolicyAreas> {
  static const bool kEmptyValueIsZero = true;
  static SchemeRegistry::PolicyAreas EmptyValue() {
    return SchemeRegistry::kPolicyAreaNone;
  }
};

class URLSchemesRegistry final {
 public:
  URLSchemesRegistry()
      :  // For ServiceWorker schemes: HTTP is required because http://localhost
         // is considered secure. Additional checks are performed to ensure that
         // other http pages are filtered out.
        service_worker_schemes({"http", "https"}),
        fetch_api_schemes({"http", "https"}),
        allowed_in_referrer_schemes({"http", "https"}) {
    for (auto& scheme : url::GetLocalSchemes())
      local_schemes.insert(scheme.c_str());
    for (auto& scheme : url::GetSecureSchemes())
      secure_schemes.insert(scheme.c_str());
    for (auto& scheme : url::GetNoAccessSchemes())
      schemes_with_unique_origins.insert(scheme.c_str());
    for (auto& scheme : url::GetCORSEnabledSchemes())
      cors_enabled_schemes.insert(scheme.c_str());
    for (auto& scheme : url::GetCSPBypassingSchemes()) {
      content_security_policy_bypassing_schemes.insert(
          scheme.c_str(), SchemeRegistry::kPolicyAreaAll);
    }
    for (auto& scheme : url::GetEmptyDocumentSchemes())
      empty_document_schemes.insert(scheme.c_str());
  }
  ~URLSchemesRegistry() = default;

  URLSchemesSet local_schemes;
  URLSchemesSet display_isolated_url_schemes;
  URLSchemesSet secure_schemes;
  URLSchemesSet schemes_with_unique_origins;
  URLSchemesSet empty_document_schemes;
  URLSchemesSet schemes_forbidden_from_domain_relaxation;
  URLSchemesSet not_allowing_javascript_urls_schemes;
  URLSchemesSet cors_enabled_schemes;
  URLSchemesSet service_worker_schemes;
  URLSchemesSet fetch_api_schemes;
  URLSchemesSet first_party_when_top_level_schemes;
  URLSchemesMap<SchemeRegistry::PolicyAreas, PolicyAreasHashTraits>
      content_security_policy_bypassing_schemes;
  URLSchemesSet secure_context_bypassing_schemes;
  URLSchemesSet allowed_in_referrer_schemes;

 private:
  friend const URLSchemesRegistry& GetURLSchemesRegistry();
  friend URLSchemesRegistry& GetMutableURLSchemesRegistry();

  static URLSchemesRegistry& GetInstance() {
    DEFINE_STATIC_LOCAL(URLSchemesRegistry, schemes, ());
    return schemes;
  }
};

const URLSchemesRegistry& GetURLSchemesRegistry() {
  return URLSchemesRegistry::GetInstance();
}

URLSchemesRegistry& GetMutableURLSchemesRegistry() {
#if DCHECK_IS_ON()
  DCHECK(WTF::IsBeforeThreadCreated());
#endif
  return URLSchemesRegistry::GetInstance();
}

}  // namespace

// Must be called before we create other threads to avoid racy static local
// initialization.
void SchemeRegistry::Initialize() {
  GetURLSchemesRegistry();
}

void SchemeRegistry::RegisterURLSchemeAsLocal(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().local_schemes.insert(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsLocal(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().local_schemes.Contains(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsNoAccess(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().schemes_with_unique_origins.Contains(scheme);
}

void SchemeRegistry::RegisterURLSchemeAsDisplayIsolated(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().display_isolated_url_schemes.insert(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsDisplayIsolated(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().display_isolated_url_schemes.Contains(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsRestrictingMixedContent(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  return scheme == "https";
}

void SchemeRegistry::RegisterURLSchemeAsSecure(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().secure_schemes.insert(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsSecure(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().secure_schemes.Contains(scheme);
}

bool SchemeRegistry::ShouldLoadURLSchemeAsEmptyDocument(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().empty_document_schemes.Contains(scheme);
}

void SchemeRegistry::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return;

  if (forbidden) {
    GetMutableURLSchemesRegistry()
        .schemes_forbidden_from_domain_relaxation.insert(scheme);
  } else {
    GetMutableURLSchemesRegistry()
        .schemes_forbidden_from_domain_relaxation.erase(scheme);
  }
}

bool SchemeRegistry::IsDomainRelaxationForbiddenForURLScheme(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry()
      .schemes_forbidden_from_domain_relaxation.Contains(scheme);
}

bool SchemeRegistry::CanDisplayOnlyIfCanRequest(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  return scheme == "blob" || scheme == "filesystem";
}

void SchemeRegistry::RegisterURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().not_allowing_javascript_urls_schemes.insert(
      scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().not_allowing_javascript_urls_schemes.Contains(
      scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().cors_enabled_schemes.Contains(scheme);
}

String SchemeRegistry::ListOfCORSEnabledURLSchemes() {
  StringBuilder builder;
  bool add_separator = false;
  for (const auto& scheme : GetURLSchemesRegistry().cors_enabled_schemes) {
    if (add_separator)
      builder.Append(", ");
    else
      add_separator = true;

    builder.Append(scheme);
  }
  return builder.ToString();
}

bool SchemeRegistry::ShouldTreatURLSchemeAsLegacy(const String& scheme) {
  return scheme == "ftp" || scheme == "gopher";
}

bool SchemeRegistry::ShouldTrackUsageMetricsForScheme(const String& scheme) {
  // The scheme represents content which likely cannot be easily updated.
  // Specifically this includes internal pages such as about, chrome-devtools,
  // etc.
  // "chrome-extension" is not included because they have a single deployment
  // point (the webstore) and are designed specifically for Chrome.
  // "data" is not included because real sites shouldn't be using it for
  // top-level
  // pages and Chrome does use it internally (eg. PluginPlaceholder).
  return scheme == "http" || scheme == "https" || scheme == "file";
}

void SchemeRegistry::RegisterURLSchemeAsAllowingServiceWorkers(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().service_worker_schemes.insert(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().service_worker_schemes.Contains(scheme);
}

void SchemeRegistry::RegisterURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().fetch_api_schemes.insert(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsSupportingFetchAPI(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().fetch_api_schemes.Contains(scheme);
}

void SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().first_party_when_top_level_schemes.insert(
      scheme);
}

void SchemeRegistry::RemoveURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().first_party_when_top_level_schemes.erase(
      scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().first_party_when_top_level_schemes.Contains(
      scheme);
}

void SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().allowed_in_referrer_schemes.insert(scheme);
}

void SchemeRegistry::RemoveURLSchemeAsAllowedForReferrer(const String& scheme) {
  GetMutableURLSchemesRegistry().allowed_in_referrer_schemes.erase(scheme);
}

bool SchemeRegistry::ShouldTreatURLSchemeAsAllowedForReferrer(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  if (scheme.IsEmpty())
    return false;
  return GetURLSchemesRegistry().allowed_in_referrer_schemes.Contains(scheme);
}

void SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policy_areas) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry()
      .content_security_policy_bypassing_schemes.insert(scheme, policy_areas);
}

void SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry()
      .content_security_policy_bypassing_schemes.erase(scheme);
}

bool SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
    const String& scheme,
    PolicyAreas policy_areas) {
  DCHECK_NE(policy_areas, kPolicyAreaNone);
  if (scheme.IsEmpty() || policy_areas == kPolicyAreaNone)
    return false;

  // get() returns 0 (PolicyAreaNone) if there is no entry in the map.
  // Thus by default, schemes do not bypass CSP.
  return (GetURLSchemesRegistry().content_security_policy_bypassing_schemes.at(
              scheme) &
          policy_areas) == policy_areas;
}

void SchemeRegistry::RegisterURLSchemeBypassingSecureContextCheck(
    const String& scheme) {
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  GetMutableURLSchemesRegistry().secure_context_bypassing_schemes.insert(
      scheme);
}

bool SchemeRegistry::SchemeShouldBypassSecureContextCheck(
    const String& scheme) {
  if (scheme.IsEmpty())
    return false;
  DCHECK_EQ(scheme, scheme.DeprecatedLower());
  return GetURLSchemesRegistry().secure_context_bypassing_schemes.Contains(
      scheme);
}

}  // namespace blink
