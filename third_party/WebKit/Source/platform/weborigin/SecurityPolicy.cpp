/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/SecurityPolicy.h"

#include <memory>
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/OriginAccessEntry.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

using OriginAccessWhiteList = Vector<OriginAccessEntry>;
using OriginAccessMap = HashMap<String, std::unique_ptr<OriginAccessWhiteList>>;
using OriginSet = HashSet<String>;

static OriginAccessMap& GetOriginAccessMap() {
  DEFINE_STATIC_LOCAL(OriginAccessMap, origin_access_map, ());
  return origin_access_map;
}

static OriginSet& TrustworthyOriginSet() {
  DEFINE_STATIC_LOCAL(OriginSet, trustworthy_origin_set, ());
  return trustworthy_origin_set;
}

void SecurityPolicy::Init() {
  GetOriginAccessMap();
  TrustworthyOriginSet();
}

bool SecurityPolicy::ShouldHideReferrer(const KURL& url, const KURL& referrer) {
  bool referrer_is_secure_url = referrer.ProtocolIs("https");
  bool scheme_is_allowed =
      SchemeRegistry::ShouldTreatURLSchemeAsAllowedForReferrer(
          referrer.Protocol());

  if (!scheme_is_allowed)
    return true;

  if (!referrer_is_secure_url)
    return false;

  bool url_is_secure_url = url.ProtocolIs("https");

  return !url_is_secure_url;
}

Referrer SecurityPolicy::GenerateReferrer(ReferrerPolicy referrer_policy,
                                          const KURL& url,
                                          const String& referrer) {
  ReferrerPolicy referrer_policy_no_default = referrer_policy;
  if (referrer_policy_no_default == kReferrerPolicyDefault) {
    if (RuntimeEnabledFeatures::ReducedReferrerGranularityEnabled()) {
      referrer_policy_no_default =
          kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      referrer_policy_no_default = kReferrerPolicyNoReferrerWhenDowngrade;
    }
  }
  if (referrer == Referrer::NoReferrer())
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
  DCHECK(!referrer.IsEmpty());

  KURL referrer_url = KURL(KURL(), referrer);
  String scheme = referrer_url.Protocol();
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowedForReferrer(scheme))
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  if (SecurityOrigin::ShouldUseInnerURL(url))
    return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);

  switch (referrer_policy_no_default) {
    case kReferrerPolicyNever:
      return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
    case kReferrerPolicyAlways:
      return Referrer(referrer, referrer_policy_no_default);
    case kReferrerPolicyOrigin: {
      String origin = SecurityOrigin::Create(referrer_url)->ToString();
      // A security origin is not a canonical URL as it lacks a path. Add /
      // to turn it into a canonical URL we can use as referrer.
      return Referrer(origin + "/", referrer_policy_no_default);
    }
    case kReferrerPolicyOriginWhenCrossOrigin: {
      RefPtr<SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      RefPtr<SecurityOrigin> url_origin = SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.Get())) {
        String origin = referrer_origin->ToString();
        return Referrer(origin + "/", referrer_policy_no_default);
      }
      break;
    }
    case kReferrerPolicySameOrigin: {
      RefPtr<SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      RefPtr<SecurityOrigin> url_origin = SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.Get())) {
        return Referrer(Referrer::NoReferrer(), referrer_policy_no_default);
      }
      return Referrer(referrer, referrer_policy_no_default);
    }
    case kReferrerPolicyStrictOrigin: {
      String origin = SecurityOrigin::Create(referrer_url)->ToString();
      return Referrer(ShouldHideReferrer(url, referrer_url)
                          ? Referrer::NoReferrer()
                          : origin + "/",
                      referrer_policy_no_default);
    }
    case kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin: {
      RefPtr<SecurityOrigin> referrer_origin =
          SecurityOrigin::Create(referrer_url);
      RefPtr<SecurityOrigin> url_origin = SecurityOrigin::Create(url);
      if (!url_origin->IsSameSchemeHostPort(referrer_origin.Get())) {
        String origin = referrer_origin->ToString();
        return Referrer(ShouldHideReferrer(url, referrer_url)
                            ? Referrer::NoReferrer()
                            : origin + "/",
                        referrer_policy_no_default);
      }
      break;
    }
    case kReferrerPolicyNoReferrerWhenDowngrade:
      break;
    case kReferrerPolicyDefault:
      NOTREACHED();
      break;
  }

  return Referrer(
      ShouldHideReferrer(url, referrer_url) ? Referrer::NoReferrer() : referrer,
      referrer_policy_no_default);
}

void SecurityPolicy::AddOriginTrustworthyWhiteList(
    PassRefPtr<SecurityOrigin> origin) {
#if DCHECK_IS_ON()
  // Must be called before we start other threads.
  DCHECK(WTF::IsBeforeThreadCreated());
#endif
  if (origin->IsUnique())
    return;
  TrustworthyOriginSet().insert(origin->ToRawString());
}

bool SecurityPolicy::IsOriginWhiteListedTrustworthy(
    const SecurityOrigin& origin) {
  // Early return if there are no whitelisted origins to avoid unnecessary
  // allocations, copies, and frees.
  if (origin.IsUnique() || TrustworthyOriginSet().IsEmpty())
    return false;
  return TrustworthyOriginSet().Contains(origin.ToRawString());
}

bool SecurityPolicy::IsUrlWhiteListedTrustworthy(const KURL& url) {
  // Early return to avoid initializing the SecurityOrigin.
  if (TrustworthyOriginSet().IsEmpty())
    return false;
  return IsOriginWhiteListedTrustworthy(*SecurityOrigin::Create(url).Get());
}

bool SecurityPolicy::IsAccessWhiteListed(const SecurityOrigin* active_origin,
                                         const SecurityOrigin* target_origin) {
  const OriginAccessMap& map = GetOriginAccessMap();
  if (map.IsEmpty())
    return false;
  if (OriginAccessWhiteList* list = map.at(active_origin->ToString())) {
    for (size_t i = 0; i < list->size(); ++i) {
      if (list->at(i).MatchesOrigin(*target_origin) !=
          OriginAccessEntry::kDoesNotMatchOrigin)
        return true;
    }
  }
  return false;
}

bool SecurityPolicy::IsAccessToURLWhiteListed(
    const SecurityOrigin* active_origin,
    const KURL& url) {
  RefPtr<SecurityOrigin> target_origin = SecurityOrigin::Create(url);
  return IsAccessWhiteListed(active_origin, target_origin.Get());
}

void SecurityPolicy::AddOriginAccessWhitelistEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    bool allow_destination_subdomains) {
  DCHECK(IsMainThread());
  DCHECK(!source_origin.IsUnique());
  if (source_origin.IsUnique())
    return;

  String source_string = source_origin.ToString();
  OriginAccessMap::AddResult result =
      GetOriginAccessMap().insert(source_string, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = WTF::WrapUnique(new OriginAccessWhiteList);

  OriginAccessWhiteList* list = result.stored_value->value.get();
  list->push_back(OriginAccessEntry(
      destination_protocol, destination_domain,
      allow_destination_subdomains ? OriginAccessEntry::kAllowSubdomains
                                   : OriginAccessEntry::kDisallowSubdomains));
}

void SecurityPolicy::RemoveOriginAccessWhitelistEntry(
    const SecurityOrigin& source_origin,
    const String& destination_protocol,
    const String& destination_domain,
    bool allow_destination_subdomains) {
  DCHECK(IsMainThread());
  DCHECK(!source_origin.IsUnique());
  if (source_origin.IsUnique())
    return;

  String source_string = source_origin.ToString();
  OriginAccessMap& map = GetOriginAccessMap();
  OriginAccessMap::iterator it = map.find(source_string);
  if (it == map.end())
    return;

  OriginAccessWhiteList* list = it->value.get();
  size_t index = list->Find(OriginAccessEntry(
      destination_protocol, destination_domain,
      allow_destination_subdomains ? OriginAccessEntry::kAllowSubdomains
                                   : OriginAccessEntry::kDisallowSubdomains));

  if (index == kNotFound)
    return;

  list->erase(index);

  if (list->IsEmpty())
    map.erase(it);
}

void SecurityPolicy::ResetOriginAccessWhitelists() {
  DCHECK(IsMainThread());
  GetOriginAccessMap().clear();
}

bool SecurityPolicy::ReferrerPolicyFromString(
    const String& policy,
    ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support,
    ReferrerPolicy* result) {
  DCHECK(!policy.IsNull());
  bool support_legacy_keywords =
      (legacy_keywords_support == kSupportReferrerPolicyLegacyKeywords);

  if (EqualIgnoringASCIICase(policy, "no-referrer") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "never"))) {
    *result = kReferrerPolicyNever;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "unsafe-url") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "always"))) {
    *result = kReferrerPolicyAlways;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin")) {
    *result = kReferrerPolicyOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "origin-when-cross-origin") ||
      (support_legacy_keywords &&
       EqualIgnoringASCIICase(policy, "origin-when-crossorigin"))) {
    *result = kReferrerPolicyOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "same-origin")) {
    *result = kReferrerPolicySameOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin")) {
    *result = kReferrerPolicyStrictOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "strict-origin-when-cross-origin")) {
    *result = kReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    return true;
  }
  if (EqualIgnoringASCIICase(policy, "no-referrer-when-downgrade") ||
      (support_legacy_keywords && EqualIgnoringASCIICase(policy, "default"))) {
    *result = kReferrerPolicyNoReferrerWhenDowngrade;
    return true;
  }
  return false;
}

bool SecurityPolicy::ReferrerPolicyFromHeaderValue(
    const String& header_value,
    ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support,
    ReferrerPolicy* result) {
  ReferrerPolicy referrer_policy = kReferrerPolicyDefault;

  Vector<String> tokens;
  header_value.Split(',', true, tokens);
  for (const auto& token : tokens) {
    ReferrerPolicy current_result;
    if (SecurityPolicy::ReferrerPolicyFromString(token.StripWhiteSpace(),
                                                 legacy_keywords_support,
                                                 &current_result)) {
      referrer_policy = current_result;
    }
  }

  if (referrer_policy == kReferrerPolicyDefault)
    return false;

  *result = referrer_policy;
  return true;
}

}  // namespace blink
