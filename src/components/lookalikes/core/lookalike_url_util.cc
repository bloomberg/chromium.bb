// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/i18n/char_iterator.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/lookalikes/core/features.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace lookalikes {

const char kHistogramName[] = "NavigationSuggestion.Event2";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kLookalikeWarningAllowlistDomains);
}

}  // namespace lookalikes

namespace {

// Digits. Used for trimming domains in Edit Distance heuristic matches. Domains
// that only differ by trailing digits (e.g. a1.tld and a2.tld) are ignored.
const char kDigitChars[] = "0123456789";

// Minimum length of e2LD protected against target embedding. For example,
// foo.bar.baz.com-evil.com embeds foo.bar.baz.com, but we don't flag it since
// "baz" is shorter than kMinTargetE2LDLength.
const size_t kMinE2LDLengthForTargetEmbedding = 4;

// This list will be added to the static list of common words so common words
// could be added to the list using a flag if needed.
const base::FeatureParam<std::string> kRemoveAdditionalCommonWords{
    &lookalikes::features::kDetectTargetEmbeddingLookalikes,
    "additional_common_words", ""};

// We might not protect a domain whose e2LD is a common word in target embedding
// based on the TLD that is paired with it. This list supplements words from
// url_formatter::common_words::IsCommonWord().
const char* kLocalAdditionalCommonWords[] = {"asahi", "hoteles", "jharkhand",
                                             "nifty"};

// These domains are plausible lookalike targets, but they also use common words
// in their names. Selectively prevent flagging embeddings where the embedder
// ends in "-DOMAIN.TLD", since these tend to have higher false positive rates.
const char* kDomainsPermittedInEndEmbeddings[] = {"office.com", "medium.com",
                                                  "orange.fr"};

// What separators can be used to separate tokens in target embedding spoofs?
// e.g. www-google.com.example.com uses "-" (www-google) and "." (google.com).
const char kTargetEmbeddingSeparators[] = "-.";

// A small subset of private registries on the PSL that act like public
// registries AND are a common source of false positives in lookalike checks. We
// treat them as public for the purposes of lookalike checks.
const char* kPrivateRegistriesTreatedAsPublic[] = {"com.de", "com.se"};

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::Contains(skeletons2, skeleton1)) {
      return true;
    }
  }
  return false;
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    const std::vector<DomainInfo>& engaged_sites,
    const DomainInfo& navigated_domain) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  for (const DomainInfo& engaged_site : engaged_sites) {
    DCHECK(!engaged_site.domain_and_registry.empty());
    if (SkeletonsMatch(navigated_domain.skeletons, engaged_site.skeletons)) {
      return engaged_site.domain_and_registry;
    }
  }
  return std::string();
}

// Scans the top sites list and returns true if it finds a domain with an edit
// distance or character swap of one to |domain_and_registry|. This search is
// done in lexicographic order on the top 500 suitable domains, instead of in
// order by popularity. This means that the resulting "similar" domain may not
// be the most popular domain that matches.
bool GetSimilarDomainFromTop500(
    const DomainInfo& navigated_domain,
    const LookalikeTargetAllowlistChecker& target_allowlisted,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const char* const top_domain_skeleton :
         top500_domains::kTop500EditDistanceSkeletons) {
      // kTop500EditDistanceSkeletons may include blank entries.
      if (strlen(top_domain_skeleton) == 0) {
        continue;
      }
      // Check edit distance on skeletons.
      if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                  base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(
                top_domain_skeleton, url_formatter::SkeletonType::kFull)
                .domain;
        DCHECK(!top_domain.empty());

        if (!IsLikelyEditDistanceFalsePositive(navigated_domain,
                                               GetDomainInfo(top_domain)) &&
            !target_allowlisted.Run(top_domain)) {
          *matched_domain = top_domain;
          *match_type = LookalikeUrlMatchType::kEditDistance;
          return true;
        }
      }

      // Check character swap on skeletons.
      // TODO(crbug/1109056): Also check character swap on actual hostnames
      // with diacritics etc removed. This is because some characters have two
      // character skeletons such as m -> rn, and this prevents us from
      // detecting character swaps between example.com and exapmle.com.
      if (HasOneCharacterSwap(base::UTF8ToUTF16(navigated_skeleton),
                              base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(
                top_domain_skeleton, url_formatter::SkeletonType::kFull)
                .domain;
        DCHECK(!top_domain.empty());
        if (!target_allowlisted.Run(top_domain)) {
          *matched_domain = top_domain;
          *match_type = LookalikeUrlMatchType::kCharacterSwapTop500;
          return true;
        }
      }
    }
  }
  return false;
}

// Scans the engaged site list for edit distance and character swap matches.
// Returns true if there is a match and fills |matched_domain| with the first
// matching engaged domain and |match_type| with the matching heuristic type.
bool GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& target_allowlisted,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      DCHECK_NE(navigated_domain.domain_and_registry,
                engaged_site.domain_and_registry);

      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      // Skip past domains that are allowed to be spoofed.
      if (target_allowlisted.Run(engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        // Check edit distance on skeletons.
        if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                    base::UTF8ToUTF16(engaged_skeleton)) &&
            !IsLikelyEditDistanceFalsePositive(navigated_domain,
                                               engaged_site)) {
          *matched_domain = engaged_site.domain_and_registry;
          *match_type = LookalikeUrlMatchType::kEditDistanceSiteEngagement;
          return true;
        }
        // Check character swap on skeletons.
        // TODO(crbug/1109056): Also check character swap on actual hostnames
        // with diacritics etc removed. This is because some characters have two
        // character skeletons such as m -> rn, and this prevents us from
        // detecting character swaps between example.com and exapmle.com.
        if (HasOneCharacterSwap(base::UTF8ToUTF16(navigated_skeleton),
                                base::UTF8ToUTF16(engaged_skeleton))) {
          *matched_domain = engaged_site.domain_and_registry;
          *match_type = LookalikeUrlMatchType::kCharacterSwapSiteEngagement;
          return true;
        }
      }
    }
  }
  return false;
}

void RecordEvent(NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(lookalikes::kHistogramName, event);
}

// Returns the parts of the domain that are separated by "." or "-", not
// including the eTLD.
//
// |hostname| must outlive the return value since the vector contains
// StringPieces.
std::vector<base::StringPiece> SplitDomainIntoTokens(
    const std::string& hostname) {
  return base::SplitStringPiece(hostname, kTargetEmbeddingSeparators,
                                base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Returns whether any subdomain ending in the last entry of |domain_labels| is
// allowlisted. e.g. if domain_labels = {foo,scholar,google,com}, checks the
// allowlist for google.com, scholar.google.com, and foo.scholar.google.com.
bool ASubdomainIsAllowlisted(
    const base::span<const base::StringPiece>& domain_labels,
    const LookalikeTargetAllowlistChecker& in_target_allowlist) {
  DCHECK(domain_labels.size() >= 2);
  std::string potential_hostname(domain_labels[domain_labels.size() - 1]);
  // Attach each token from the end to the embedded target to check if that
  // subdomain has been allowlisted.
  for (int i = domain_labels.size() - 2; i >= 0; i--) {
    potential_hostname =
        std::string(domain_labels[i]) + "." + potential_hostname;
    if (in_target_allowlist.Run(potential_hostname)) {
      return true;
    }
  }
  return false;
}

// Returns the top domain if the top domain without its separators matches the
// |potential_target| (e.g. googlecom). The matching is a skeleton matching.
std::string GetMatchingTopDomainWithoutSeparators(
    const base::StringPiece& potential_target) {
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(base::UTF8ToUTF16(potential_target));

  for (const auto& skeleton : skeletons) {
    url_formatter::TopDomainEntry matched_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kSeparatorsRemoved);
    if (!matched_domain.domain.empty() &&
        matched_domain.skeleton_type ==
            url_formatter::SkeletonType::kSeparatorsRemoved) {
      return matched_domain.domain;
    }
  }
  return std::string();
}

// Returns whether the visited domain is either for a bare eTLD+1 (e.g.
// 'google.com') or a trivial subdomain (e.g. 'www.google.com').
bool IsETLDPlusOneOrTrivialSubdomain(const DomainInfo& host) {
  return (host.domain_and_registry == host.hostname ||
          "www." + host.domain_and_registry == host.hostname);
}

// Returns if |etld_plus_one| shares the skeleton of an eTLD+1 with an engaged
// site or a top 500 domain. |embedded_target| is set to matching eTLD+1.
bool DoesETLDPlus1MatchTopDomainOrEngagedSite(
    const DomainInfo& domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* embedded_target) {
  for (const auto& skeleton : domain.skeletons) {
    for (const auto& engaged_site : engaged_sites) {
      // Skeleton matching only calculates skeletons of the eTLD+1, so only
      // consider engaged sites that are bare eTLD+1s (or a trivial subdomain)
      // and are a skeleton match.
      if (IsETLDPlusOneOrTrivialSubdomain(engaged_site) &&
          base::Contains(engaged_site.skeletons, skeleton)) {
        *embedded_target = engaged_site.domain_and_registry;
        return true;
      }
    }
  }
  for (const auto& skeleton : domain.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (!top_domain.domain.empty() && top_domain.is_top_500) {
      *embedded_target = top_domain.domain;
      return true;
    }
  }
  return false;
}

// Returns whether the e2LD of the provided domain is a common word (e.g.
// weather.com, ask.com). Target embeddings of these domains are often false
// positives (e.g. "super-best-fancy-hotels.com" isn't spoofing "hotels.com").
bool UsesCommonWord(const reputation::SafetyTipsConfig* config_proto,
                    const DomainInfo& domain) {
  // kDomainsPermittedInEndEmbeddings are based on domains with common words,
  // but they should not be excluded here (and instead are checked later).
  for (auto* permitted_ending : kDomainsPermittedInEndEmbeddings) {
    if (domain.domain_and_registry == permitted_ending) {
      return false;
    }
  }

  // Search for words in the big common word list.
  if (url_formatter::common_words::IsCommonWord(
          domain.domain_without_registry)) {
    return true;
  }

  // Search for words in the component-provided word list.
  if (reputation::IsCommonWordInConfigProto(config_proto,
                                            domain.domain_without_registry)) {
    return true;
  }

  // Search for words in the local word lists.
  for (auto* common_word : kLocalAdditionalCommonWords) {
    if (domain.domain_without_registry == common_word) {
      return true;
    }
  }
  std::vector<std::string> additional_common_words =
      base::SplitString(kRemoveAdditionalCommonWords.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(additional_common_words, domain.domain_without_registry)) {
    return true;
  }

  return false;
}

// Returns whether |domain_labels| is in the same domain as embedding_domain.
// e.g. IsEmbeddingItself(["foo", "example", "com"], "example.com") -> true
//  since foo.example.com is in the same domain as example.com.
bool IsEmbeddingItself(const base::span<const base::StringPiece>& domain_labels,
                       const std::string& embedding_domain) {
  DCHECK(domain_labels.size() >= 2);
  std::string potential_hostname(domain_labels[domain_labels.size() - 1]);
  // Attach each token from the end to the embedded target to check if that
  // subdomain is the embedding domain. (e.g. using the earlier example, check
  // each ["com", "example.com", "foo.example.com"] against "example.com".
  for (int i = domain_labels.size() - 2; i >= 0; i--) {
    potential_hostname =
        std::string(domain_labels[i]) + "." + potential_hostname;
    if (embedding_domain == potential_hostname) {
      return true;
    }
  }
  return false;
}

// Identical to url_formatter::top_domains::HostnameWithoutRegistry(), but
// respects de-facto public registries like .com.de using similar logic to
// GetETLDPlusOne. See kPrivateRegistriesTreatedAsPublic definition for more
// details. e.g. "google.com.de" returns "google". Call with an eTLD+1, not a
// full hostname.
std::string GetE2LDWithDeFactoPublicRegistries(
    const std::string& domain_and_registry) {
  if (domain_and_registry.empty()) {
    return std::string();
  }

  size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          domain_and_registry.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  const size_t private_registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          domain_and_registry.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // If the registry lengths are the same using public and private registries,
  // than this is just a public registry domain. Otherwise, we need to check if
  // the registry ends with one of our anointed registries.
  if (registry_size != private_registry_size) {
    for (const auto* private_registry : kPrivateRegistriesTreatedAsPublic) {
      if (base::EndsWith(domain_and_registry, private_registry)) {
        registry_size = private_registry_size;
      }
    }
  }

  std::string out =
      domain_and_registry.substr(0, domain_and_registry.size() - registry_size);
  base::TrimString(out, ".", &out);
  return out;
}

// Returns whether |embedded_target| and |embedding_domain| share the same e2LD,
// (as in, e.g., google.com and google.org, or airbnb.com.br and airbnb.com).
// Assumes |embedding_domain| is an eTLD+1. Respects de-facto public eTLDs.
bool IsCrossTLDMatch(const DomainInfo& embedded_target,
                     const std::string& embedding_domain) {
  return (
      GetE2LDWithDeFactoPublicRegistries(embedded_target.domain_and_registry) ==
      GetE2LDWithDeFactoPublicRegistries(embedding_domain));
}

// Returns whether |embedded_target| is one of kDomainsPermittedInEndEmbeddings
// and that |embedding_domain| ends with that domain, e.g. "evil-office.com" is
// permitted, as "office.com" is in kDomainsPermittedInEndEmbeddings.  Only
// impacts Target Embedding matches.
bool EndsWithPermittedDomains(const DomainInfo& embedded_target,
                              const std::string& embedding_domain) {
  for (auto* permitted_ending : kDomainsPermittedInEndEmbeddings) {
    if (embedded_target.domain_and_registry == permitted_ending &&
        base::EndsWith(embedding_domain,
                       base::StrCat({"-", permitted_ending}))) {
      return true;
    }
  }
  return false;
}

// A domain is allowed to be embedded if is embedding itself, if its e2LD is a
// common word, any valid partial subdomain is allowlisted, or if it's a
// cross-TLD match (e.g. google.com vs google.com.mx).
bool IsAllowedToBeEmbedded(
    const DomainInfo& embedded_target,
    const base::span<const base::StringPiece>& subdomain_span,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const std::string& embedding_domain,
    const reputation::SafetyTipsConfig* config_proto) {
  return UsesCommonWord(config_proto, embedded_target) ||
         ASubdomainIsAllowlisted(subdomain_span, in_target_allowlist) ||
         IsEmbeddingItself(subdomain_span, embedding_domain) ||
         IsCrossTLDMatch(embedded_target, embedding_domain) ||
         EndsWithPermittedDomains(embedded_target, embedding_domain);
}

// Returns the first character of the first string that is different from the
// second string. Strings should be at least 1 edit distance apart.
char GetFirstDifferentChar(const std::string& str1, const std::string& str2) {
  std::string::const_iterator i1 = str1.begin();
  std::string::const_iterator i2 = str2.begin();
  while (i1 != str1.end() && i2 != str2.end()) {
    if (*i1 != *i2) {
      return *i1;
    }
    i1++;
    i2++;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

DomainInfo::DomainInfo(const std::string& arg_hostname,
                       const std::string& arg_domain_and_registry,
                       const std::string& arg_domain_without_registry,
                       const url_formatter::IDNConversionResult& arg_idn_result,
                       const url_formatter::Skeletons& arg_skeletons)
    : hostname(arg_hostname),
      domain_and_registry(arg_domain_and_registry),
      domain_without_registry(arg_domain_without_registry),
      idn_result(arg_idn_result),
      skeletons(arg_skeletons) {}

DomainInfo::~DomainInfo() = default;

DomainInfo::DomainInfo(const DomainInfo&) = default;

DomainInfo GetDomainInfo(const std::string& hostname) {
  TRACE_EVENT0("navigation", "GetDomainInfo");
  if (net::HostStringIsLocalhost(hostname) ||
      net::IsHostnameNonUnique(hostname)) {
    return DomainInfo(std::string(), std::string(), std::string(),
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  const std::string domain_and_registry = GetETLDPlusOne(hostname);
  const std::string domain_without_registry =
      domain_and_registry.empty()
          ? std::string()
          : url_formatter::top_domains::HostnameWithoutRegistry(
                domain_and_registry);

  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty()) {
    return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  // Compute skeletons using eTLD+1, skipping all spoofing checks. Spoofing
  // checks in url_formatter can cause the converted result to be punycode.
  // We want to avoid this in order to get an accurate skeleton for the unicode
  // version of the domain.
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_and_registry);
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(idn_result.result);
  return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                    idn_result, skeletons);
}

DomainInfo GetDomainInfo(const GURL& url) {
  return GetDomainInfo(url.host());
}

std::string GetETLDPlusOne(const std::string& hostname) {
  auto pub = net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  auto priv = net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // If there is no difference in eTLD+1 with/without private registries, then
  // the domain uses a public registry and we can return the eTLD+1 safely.
  if (pub == priv) {
    return pub;
  }
  // Otherwise, the domain uses a private registry and |pub| is that private
  // registry. If it's a de-facto-public registry, return the private eTLD+1.
  for (auto* private_registry : kPrivateRegistriesTreatedAsPublic) {
    if (private_registry == pub) {
      return priv;
    }
  }
  // Otherwise, ignore the normal private registry and return the public eTLD+1.
  return pub;
}

bool IsEditDistanceAtMostOne(const std::u16string& str1,
                             const std::u16string& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  std::u16string::const_iterator i = str1.begin();
  std::u16string::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

bool IsLikelyEditDistanceFalsePositive(const DomainInfo& navigated_domain,
                                       const DomainInfo& matched_domain) {
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      matched_domain.domain_and_registry));
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      navigated_domain.domain_and_registry));
  // If the only difference between the domains is the registry part, this is
  // unlikely to be a spoofing attempt and we should ignore this match.  E.g.
  // exclude matches like google.com.tw and google.com.tr.
  if (navigated_domain.domain_without_registry ==
      matched_domain.domain_without_registry) {
    return true;
  }

  // If the domains only differ by a numeric suffix on their e2LD (e.g.
  // site45.tld and site35.tld), then ignore the match.
  auto nav_trimmed = base::TrimString(navigated_domain.domain_without_registry,
                                      kDigitChars, base::TRIM_TRAILING);
  auto matched_trimmed = base::TrimString(
      matched_domain.domain_without_registry, kDigitChars, base::TRIM_TRAILING);
  DCHECK_NE(navigated_domain.domain_without_registry,
            matched_domain.domain_without_registry);
  // We previously verified that the domains without registries weren't equal,
  // so if they're equal now, the match must have come from numeric suffixes.
  if (nav_trimmed == matched_trimmed) {
    return true;
  }

  // Ignore domains that only differ by an insertion/substitution at the
  // start, as these are usually different words, not lookalikes.
  const auto nav_dom_len = navigated_domain.domain_and_registry.length();
  const auto matched_dom_len = matched_domain.domain_and_registry.length();
  const auto& nav_dom = navigated_domain.domain_and_registry;
  const auto& matched_dom = matched_domain.domain_and_registry;
  if (nav_dom_len == matched_dom_len) {
    // e.g. hank vs tank
    if (nav_dom.substr(1) == matched_dom.substr(1)) {
      return true;
    }
  } else if (nav_dom_len < matched_dom_len) {
    // e.g. oodle vs poodle
    if (nav_dom == matched_dom.substr(1)) {
      return true;
    }
  } else {  // navigated_dom_len > matched_dom_len
    // e.g. poodle vs oodle
    if (nav_dom.substr(1) == matched_dom) {
      return true;
    }
  }

  // Ignore domains that only differ by an insertion of a "-".
  if (nav_dom_len != matched_dom_len) {
    if (nav_dom_len < matched_dom_len &&
        GetFirstDifferentChar(matched_dom, nav_dom) == '-') {
      return true;
    } else if (nav_dom_len > matched_dom_len &&
               GetFirstDifferentChar(nav_dom, matched_dom) == '-') {
      return true;
    }
  }

  return false;
}

bool IsTopDomain(const DomainInfo& domain_info) {
  // Top domains are only accessible through their skeletons, so query the top
  // domains trie for each skeleton of this domain.
  for (const std::string& skeleton : domain_info.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (domain_info.domain_and_registry == top_domain.domain) {
      return true;
    }
  }
  return false;
}

bool ShouldBlockLookalikeUrlNavigation(LookalikeUrlMatchType match_type) {
  if (match_type == LookalikeUrlMatchType::kSiteEngagement) {
    return true;
  }
  if (match_type == LookalikeUrlMatchType::kTargetEmbedding) {
#if defined(OS_IOS)
    // TODO(crbug.com/1104384): Only enable target embedding on iOS once we can
    //    check engaged sites. Otherwise, false positives are too high.
    return false;
#else
    return base::FeatureList::IsEnabled(
        lookalikes::features::kDetectTargetEmbeddingLookalikes);
#endif
  }
  if (match_type == LookalikeUrlMatchType::kFailedSpoofChecks &&
      base::FeatureList::IsEnabled(
          lookalikes::features::kLookalikeInterstitialForPunycode)) {
    return true;
  }
  return match_type == LookalikeUrlMatchType::kSkeletonMatchTop500;
}

bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  DCHECK(matched_domain);
  DCHECK(match_type);

  if (navigated_domain.idn_result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against engaged sites
    // and top domains.
    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(engaged_sites, navigated_domain);
    DCHECK_NE(navigated_domain.domain_and_registry, matched_engaged_domain);
    if (!matched_engaged_domain.empty()) {
      *matched_domain = matched_engaged_domain;
      *match_type = LookalikeUrlMatchType::kSiteEngagement;
      return true;
    }

    if (!navigated_domain.idn_result.matching_top_domain.domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      // At this point, navigated domain should not be a top domain.
      DCHECK_NE(navigated_domain.domain_and_registry,
                navigated_domain.idn_result.matching_top_domain.domain);
      *matched_domain = navigated_domain.idn_result.matching_top_domain.domain;
      *match_type = navigated_domain.idn_result.matching_top_domain.is_top_500
                        ? LookalikeUrlMatchType::kSkeletonMatchTop500
                        : LookalikeUrlMatchType::kSkeletonMatchTop5k;
      return true;
    }
  }

  if (url_formatter::top_domains::IsEditDistanceCandidate(
          navigated_domain.domain_and_registry)) {
    // If we can't find an exact top domain or an engaged site, try to find an
    // engaged domain within an edit distance of one or a single character swap.
    if (GetSimilarDomainFromEngagedSites(navigated_domain, engaged_sites,
                                         in_target_allowlist, matched_domain,
                                         match_type)) {
      DCHECK_NE(navigated_domain.domain_and_registry, *matched_domain);
      return true;
    }

    // Finally, try to find a top domain within an edit distance or character
    // swap of one.
    if (GetSimilarDomainFromTop500(navigated_domain, in_target_allowlist,
                                   matched_domain, match_type)) {
      DCHECK_NE(navigated_domain.domain_and_registry, *matched_domain);
      DCHECK(!matched_domain->empty());
      return true;
    }
  }

  TargetEmbeddingType embedding_type =
      GetTargetEmbeddingType(navigated_domain.hostname, engaged_sites,
                             in_target_allowlist, config_proto, matched_domain);
  if (embedding_type == TargetEmbeddingType::kSafetyTip) {
    *match_type = LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips;
    return true;
  } else if (embedding_type == TargetEmbeddingType::kInterstitial) {
    *match_type = LookalikeUrlMatchType::kTargetEmbedding;
    return true;
  }

  DCHECK(embedding_type == TargetEmbeddingType::kNone);
  return false;
}

void RecordUMAFromMatchType(LookalikeUrlMatchType match_type) {
  switch (match_type) {
    case LookalikeUrlMatchType::kSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
      break;
    case LookalikeUrlMatchType::kEditDistance:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistance);
      break;
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistanceSiteEngagement);
      break;
    case LookalikeUrlMatchType::kTargetEmbedding:
      RecordEvent(NavigationSuggestionEvent::kMatchTargetEmbedding);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop500);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop5k);
      break;
    case LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips:
      RecordEvent(
          NavigationSuggestionEvent::kMatchTargetEmbeddingForSafetyTips);
      break;
    case LookalikeUrlMatchType::kFailedSpoofChecks:
      RecordEvent(NavigationSuggestionEvent::kFailedSpoofChecks);
      break;
    case LookalikeUrlMatchType::kCharacterSwapSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchCharacterSwapSiteEngagement);
      break;
    case LookalikeUrlMatchType::kCharacterSwapTop500:
      RecordEvent(NavigationSuggestionEvent::kMatchCharacterSwapTop500);
      break;
    case LookalikeUrlMatchType::kNone:
      break;
  }
}

TargetEmbeddingType GetTargetEmbeddingType(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* safe_hostname) {
  // Because of how target embeddings are detected (i.e. by sweeping the URL
  // from back to front), we're guaranteed to find tail-embedding before other
  // target embedding. Tail embedding triggers a safety tip, but interstitials
  // are more important than safety tips, so if we find a safety tippable
  // embedding with SearchForEmbeddings, go search again not permitting safety
  // tips to see if we can also find an interstitiallable embedding.
  auto result = SearchForEmbeddings(
      hostname, engaged_sites, in_target_allowlist, config_proto,
      /*safety_tips_allowed=*/true, safe_hostname);
  if (result == TargetEmbeddingType::kSafetyTip) {
    std::string no_st_safe_hostname;
    auto no_st_result = SearchForEmbeddings(
        hostname, engaged_sites, in_target_allowlist, config_proto,
        /*safety_tips_allowed=*/false, &no_st_safe_hostname);
    if (no_st_result == TargetEmbeddingType::kNone) {
      return result;
    }
    *safe_hostname = no_st_safe_hostname;
    return no_st_result;
  }
  return result;
}

TargetEmbeddingType SearchForEmbeddings(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    bool safety_tips_allowed,
    std::string* safe_hostname) {
  const std::string embedding_domain = GetETLDPlusOne(hostname);
  const std::vector<base::StringPiece> hostname_tokens =
      SplitDomainIntoTokens(hostname);

  // There are O(n^2) potential target embeddings in a domain name. We want to
  // be comprehensive, but optimize so that usually we needn't check all of
  // them. We do that by sweeping from the back of the embedding domain, towards
  // the front, checking for a valid eTLD. If we find one, then we consider the
  // possible embedded domains that end in that eTLD (i.e. all possible start
  // points from the beginning of the string onward).
  for (int end = hostname_tokens.size(); end > 0; --end) {
    base::span<const base::StringPiece> etld_check_span(hostname_tokens.data(),
                                                        end);
    std::string etld_check_host = base::JoinString(etld_check_span, ".");
    auto etld_check_dominfo = GetDomainInfo(etld_check_host);

    // Check if the final token is a no-separator target (e.g. "googlecom").
    // This check happens first so that we can exclude invalid eTLD+1s next.
    std::string embedded_target =
        GetMatchingTopDomainWithoutSeparators(hostname_tokens[end - 1]);
    if (!embedded_target.empty()) {
      // Extract the full possibly-spoofed domain. To get this, we take the
      // hostname up until this point, strip off the no-separator bit (e.g.
      // googlecom) and then re-add the the separated version (e.g. google.com).
      auto spoofed_domain =
          etld_check_host.substr(
              0, etld_check_host.length() - hostname_tokens[end - 1].length()) +
          embedded_target;
      const auto no_separator_tokens = base::SplitStringPiece(
          spoofed_domain, kTargetEmbeddingSeparators, base::TRIM_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      auto no_separator_dominfo = GetDomainInfo(embedded_target);

      // Only flag on domains that are long enough, don't use common words, and
      // aren't target-allowlisted.
      if (no_separator_dominfo.domain_without_registry.length() >
              kMinE2LDLengthForTargetEmbedding &&
          !IsAllowedToBeEmbedded(no_separator_dominfo, no_separator_tokens,
                                 in_target_allowlist, embedding_domain,
                                 config_proto)) {
        *safe_hostname = embedded_target;
        return TargetEmbeddingType::kInterstitial;
      }
    }

    // Exclude otherwise-invalid eTLDs.
    if (etld_check_dominfo.domain_without_registry.empty()) {
      continue;
    }

    // Exclude e2LDs that are too short. <= because domain_without_registry has
    // a trailing ".".
    if (etld_check_dominfo.domain_without_registry.length() <=
        kMinE2LDLengthForTargetEmbedding) {
      continue;
    }

    // Check for exact matches against engaged sites, among all possible
    // subdomains ending at |end|.
    for (int start = 0; start < end - 1; ++start) {
      const base::span<const base::StringPiece> span(
          hostname_tokens.data() + start, end - start);
      auto embedded_hostname = base::JoinString(span, ".");
      auto embedded_dominfo = GetDomainInfo(embedded_hostname);

      for (auto& engaged_site : engaged_sites) {
        if (engaged_site.hostname == embedded_dominfo.hostname &&
            !IsAllowedToBeEmbedded(embedded_dominfo, span, in_target_allowlist,
                                   embedding_domain, config_proto)) {
          *safe_hostname = engaged_site.hostname;
          // Tail-embedding (e.g. evil-google.com, where the embedding happens
          // at the very end of the hostname) is a safety tip, but only when
          // safety tips are allowed. If it's tail embedding but we can't create
          // a safety tip, keep looking.  Non-tail-embeddings are interstitials.
          if (end != static_cast<int>(hostname_tokens.size())) {
            return TargetEmbeddingType::kInterstitial;
          } else if (safety_tips_allowed) {
            return TargetEmbeddingType::kSafetyTip;
          }  // else keep searching.
        }
      }
    }

    // There were no exact engaged site matches, but there may yet still be a
    // match against the eTLD+1 of an engaged or top site.
    if (DoesETLDPlus1MatchTopDomainOrEngagedSite(
            etld_check_dominfo, engaged_sites, safe_hostname) &&
        !IsAllowedToBeEmbedded(etld_check_dominfo, etld_check_span,
                               in_target_allowlist, embedding_domain,
                               config_proto)) {
      // Tail-embedding (e.g. evil-google.com, where the embedding happens at
      // the very end of the hostname) is a safety tip, but only when safety
      // tips are allowed. If it's tail embedding but we can't create a safety
      // tip, keep looking.  Non-tail-embeddings are interstitials.
      if (end != static_cast<int>(hostname_tokens.size())) {
        return TargetEmbeddingType::kInterstitial;
      } else if (safety_tips_allowed) {
        return TargetEmbeddingType::kSafetyTip;
      }  // else keep searching.
    }
  }
  return TargetEmbeddingType::kNone;
}

bool IsASCII(UChar32 codepoint) {
  return !(codepoint & ~0x7F);
}

// Returns true if |codepoint| has emoji related properties.
bool IsEmojiRelatedCodepoint(UChar32 codepoint) {
  return u_hasBinaryProperty(codepoint, UCHAR_EMOJI) ||
         // Characters that have emoji presentation by default (e.g. hourglass)
         u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION) ||
         // Characters displayed as country flags when used as a valid pair.
         // E.g. Regional Indicator Symbol Letter B used once in a string
         // is rendered as 🇧, used twice is rendered as the flag of Barbados
         // (with country code BB). It's therefore possible to come up with
         // a spoof using regional indicator characters as text, but these
         // domain names will be readily punycoded and detecting pairs isn't
         // easy so we keep the code simple here.
         u_hasBinaryProperty(codepoint, UCHAR_REGIONAL_INDICATOR) ||
         // Pictographs such as Black Cross On Shield (U+26E8).
         u_hasBinaryProperty(codepoint, UCHAR_EXTENDED_PICTOGRAPHIC);
}

// Returns true if |text| contains only ASCII characters, pictographs
// or emojis. This check is only used to determine if a domain that already
// failed spoof checks should be blocked by an interstitial. Ideally, we would
// check this for non-ASCII scripts as well (e.g. Cyrillic + emoji), but such
// usage isn't common.
bool IsASCIIAndEmojiOnly(const base::StringPiece16& text) {
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    const UChar32 codepoint = iter.get();
    if (!IsASCII(codepoint) && !IsEmojiRelatedCodepoint(codepoint)) {
      return false;
    }
  }
  return true;
}

bool ShouldBlockBySpoofCheckResult(const DomainInfo& navigated_domain) {
  // Here, only a subset of spoof checks that cause an IDN to fallback to
  // punycode are configured to show an interstitial.
  switch (navigated_domain.idn_result.spoof_check_result) {
    case url_formatter::IDNSpoofChecker::Result::kNone:
    case url_formatter::IDNSpoofChecker::Result::kSafe:
      return false;

    case url_formatter::IDNSpoofChecker::Result::kICUSpoofChecks:
      // If the eTLD+1 contains only a mix of ASCII + Emoji, allow.
      return !IsASCIIAndEmojiOnly(navigated_domain.idn_result.result);

    case url_formatter::IDNSpoofChecker::Result::kDeviationCharacters:
      // Failures because of deviation characters, especially ß, is common.
      return false;

    case url_formatter::IDNSpoofChecker::Result::kTLDSpecificCharacters:
    case url_formatter::IDNSpoofChecker::Result::kUnsafeMiddleDot:
    case url_formatter::IDNSpoofChecker::Result::kWholeScriptConfusable:
    case url_formatter::IDNSpoofChecker::Result::kDigitLookalikes:
    case url_formatter::IDNSpoofChecker::Result::
        kNonAsciiLatinCharMixedWithNonLatin:
    case url_formatter::IDNSpoofChecker::Result::kDangerousPattern:
      return true;
  }
}

bool IsAllowedByEnterprisePolicy(const PrefService* pref_service,
                                 const GURL& url) {
  const auto* list =
      pref_service->GetList(prefs::kLookalikeWarningAllowlistDomains);
  for (const auto& domain_val : list->GetList()) {
    auto domain = domain_val.GetString();
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

void SetEnterpriseAllowlistForTesting(PrefService* pref_service,
                                      const std::vector<std::string>& hosts) {
  base::Value list(base::Value::Type::LIST);
  for (const auto& host : hosts) {
    list.Append(host);
  }
  pref_service->Set(prefs::kLookalikeWarningAllowlistDomains, std::move(list));
}

bool HasOneCharacterSwap(const std::u16string& str1,
                         const std::u16string& str2) {
  if (str1.size() != str2.size()) {
    return false;
  }
  if (str1 == str2) {
    return false;
  }
  bool has_swap = false;
  std::u16string::const_iterator i = str1.begin();
  std::u16string::const_iterator j = str2.begin();
  while (i != str1.end()) {
    DCHECK(j < str2.end());
    wchar_t left1 = *i;
    wchar_t right1 = *j;
    i++;
    j++;
    if (left1 == right1) {
      continue;
    }
    wchar_t left2 = *i;
    wchar_t right2 = *j;
    if (!has_swap && (left1 == right2 && right1 == left2)) {
      has_swap = true;
      i++;
      j++;
      continue;
    }
    // Either there are multiple swaps, or strings have completely different
    // characters.
    return false;
  }
  return has_swap;
}
