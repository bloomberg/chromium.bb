// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_

#include <bitset>
#include <vector>

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/same_party_context.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/cookie_partition_key.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::CookiePriority, net::CookiePriority> {
  static network::mojom::CookiePriority ToMojom(net::CookiePriority input);
  static bool FromMojom(network::mojom::CookiePriority input,
                        net::CookiePriority* output);
};

template <>
struct EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite> {
  static network::mojom::CookieSameSite ToMojom(net::CookieSameSite input);
  static bool FromMojom(network::mojom::CookieSameSite input,
                        net::CookieSameSite* output);
};

template <>
struct EnumTraits<network::mojom::CookieEffectiveSameSite,
                  net::CookieEffectiveSameSite> {
  static network::mojom::CookieEffectiveSameSite ToMojom(
      net::CookieEffectiveSameSite input);
  static bool FromMojom(network::mojom::CookieEffectiveSameSite input,
                        net::CookieEffectiveSameSite* output);
};

template <>
struct EnumTraits<network::mojom::CookieAccessSemantics,
                  net::CookieAccessSemantics> {
  static network::mojom::CookieAccessSemantics ToMojom(
      net::CookieAccessSemantics input);
  static bool FromMojom(network::mojom::CookieAccessSemantics input,
                        net::CookieAccessSemantics* output);
};

template <>
struct EnumTraits<network::mojom::ContextType,
                  net::CookieOptions::SameSiteCookieContext::ContextType> {
  static network::mojom::ContextType ToMojom(
      net::CookieOptions::SameSiteCookieContext::ContextType input);
  static bool FromMojom(
      network::mojom::ContextType input,
      net::CookieOptions::SameSiteCookieContext::ContextType* output);
};

template <>
struct EnumTraits<network::mojom::CookieSameSiteContextMetadataDowngradeType,
                  net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                      ContextDowngradeType> {
  static network::mojom::CookieSameSiteContextMetadataDowngradeType ToMojom(
      net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType input);
  static bool FromMojom(
      network::mojom::CookieSameSiteContextMetadataDowngradeType input,
      net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType* output);
};

template <>
struct EnumTraits<network::mojom::CookieSourceScheme, net::CookieSourceScheme> {
  static network::mojom::CookieSourceScheme ToMojom(
      net::CookieSourceScheme input);

  static bool FromMojom(network::mojom::CookieSourceScheme input,
                        net::CookieSourceScheme* output);
};

template <>
struct EnumTraits<network::mojom::CookieChangeCause, net::CookieChangeCause> {
  static network::mojom::CookieChangeCause ToMojom(
      net::CookieChangeCause input);

  static bool FromMojom(network::mojom::CookieChangeCause input,
                        net::CookieChangeCause* output);
};

template <>
struct StructTraits<
    network::mojom::CookieSameSiteContextMetadataDataView,
    net::CookieOptions::SameSiteCookieContext::ContextMetadata> {
  static net::CookieOptions::SameSiteCookieContext::ContextMetadata::
      ContextDowngradeType
      cross_site_redirect_downgrade(
          const net::CookieOptions::SameSiteCookieContext::ContextMetadata& m) {
    return m.cross_site_redirect_downgrade;
  }

  static bool Read(network::mojom::CookieSameSiteContextMetadataDataView,
                   net::CookieOptions::SameSiteCookieContext::ContextMetadata*);
};

template <>
struct StructTraits<network::mojom::CookieSameSiteContextDataView,
                    net::CookieOptions::SameSiteCookieContext> {
  static net::CookieOptions::SameSiteCookieContext::ContextType context(
      const net::CookieOptions::SameSiteCookieContext& s) {
    return s.context();
  }

  static net::CookieOptions::SameSiteCookieContext::ContextType
  schemeful_context(const net::CookieOptions::SameSiteCookieContext& s) {
    return s.schemeful_context();
  }

  static const net::CookieOptions::SameSiteCookieContext::ContextMetadata&
  metadata(const net::CookieOptions::SameSiteCookieContext& s) {
    return s.metadata();
  }

  static const net::CookieOptions::SameSiteCookieContext::ContextMetadata&
  schemeful_metadata(const net::CookieOptions::SameSiteCookieContext& s) {
    return s.schemeful_metadata();
  }

  static bool Read(network::mojom::CookieSameSiteContextDataView mojo_options,
                   net::CookieOptions::SameSiteCookieContext* context);
};

template <>
struct EnumTraits<network::mojom::SamePartyCookieContextType,
                  net::SamePartyContext::Type> {
  static network::mojom::SamePartyCookieContextType ToMojom(
      net::SamePartyContext::Type context_type);

  static bool FromMojom(network::mojom::SamePartyCookieContextType context_type,
                        net::SamePartyContext::Type* out);
};

template <>
struct StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions> {
  static bool exclude_httponly(const net::CookieOptions& o) {
    return o.exclude_httponly();
  }
  static net::CookieOptions::SameSiteCookieContext same_site_cookie_context(
      const net::CookieOptions& o) {
    return o.same_site_cookie_context();
  }
  static bool update_access_time(const net::CookieOptions& o) {
    return o.update_access_time();
  }
  static bool return_excluded_cookies(const net::CookieOptions& o) {
    return o.return_excluded_cookies();
  }

  static net::SamePartyContext same_party_context(const net::CookieOptions& o) {
    return o.same_party_context();
  }

  static uint32_t full_party_context_size(const net::CookieOptions& o) {
    return o.full_party_context_size();
  }

  static bool is_in_nontrivial_first_party_set(const net::CookieOptions& o) {
    return o.is_in_nontrivial_first_party_set();
  }

  static bool Read(network::mojom::CookieOptionsDataView mojo_options,
                   net::CookieOptions* cookie_options);
};

template <>
struct StructTraits<network::mojom::CookiePartitionKeyDataView,
                    net::CookiePartitionKey> {
  static const net::SchemefulSite& site(const net::CookiePartitionKey& cpk) {
    return cpk.site();
  }
  static bool from_script(const net::CookiePartitionKey& cpk) {
    return cpk.from_script();
  }

  static const absl::optional<base::UnguessableToken>& nonce(
      const net::CookiePartitionKey& cpk) {
    return cpk.nonce();
  }

  static bool Read(network::mojom::CookiePartitionKeyDataView partition_key,
                   net::CookiePartitionKey* out);
};

template <>
struct StructTraits<network::mojom::CookiePartitionKeyCollectionDataView,
                    net::CookiePartitionKeyCollection> {
  static bool contains_all_partitions(
      const net::CookiePartitionKeyCollection& key_collection) {
    return key_collection.ContainsAllKeys();
  }
  static const std::vector<net::CookiePartitionKey> keys(
      const net::CookiePartitionKeyCollection& key_collection);

  static bool Read(network::mojom::CookiePartitionKeyCollectionDataView
                       key_collection_data_view,
                   net::CookiePartitionKeyCollection* out);
};

template <>
struct StructTraits<network::mojom::CanonicalCookieDataView,
                    net::CanonicalCookie> {
  static const std::string& name(const net::CanonicalCookie& c) {
    return c.Name();
  }
  static const std::string& value(const net::CanonicalCookie& c) {
    return c.Value();
  }
  static const std::string& domain(const net::CanonicalCookie& c) {
    return c.Domain();
  }
  static const std::string& path(const net::CanonicalCookie& c) {
    return c.Path();
  }
  static base::Time creation(const net::CanonicalCookie& c) {
    return c.CreationDate();
  }
  static base::Time expiry(const net::CanonicalCookie& c) {
    return c.ExpiryDate();
  }
  static base::Time last_access(const net::CanonicalCookie& c) {
    return c.LastAccessDate();
  }
  static bool secure(const net::CanonicalCookie& c) { return c.IsSecure(); }
  static bool httponly(const net::CanonicalCookie& c) { return c.IsHttpOnly(); }
  static net::CookieSameSite site_restrictions(const net::CanonicalCookie& c) {
    return c.SameSite();
  }
  static net::CookiePriority priority(const net::CanonicalCookie& c) {
    return c.Priority();
  }
  static net::CookieSourceScheme source_scheme(const net::CanonicalCookie& c) {
    return c.SourceScheme();
  }
  static bool same_party(const net::CanonicalCookie& c) {
    return c.IsSameParty();
  }
  static absl::optional<net::CookiePartitionKey> partition_key(
      const net::CanonicalCookie& c) {
    return c.PartitionKey();
  }
  static int source_port(const net::CanonicalCookie& c) {
    return c.SourcePort();
  }

  static bool Read(network::mojom::CanonicalCookieDataView cookie,
                   net::CanonicalCookie* out);
};

template <>
struct StructTraits<network::mojom::CookieInclusionStatusDataView,
                    net::CookieInclusionStatus> {
  static uint32_t exclusion_reasons(const net::CookieInclusionStatus& s) {
    return static_cast<uint32_t>(s.exclusion_reasons().to_ulong());
  }
  static uint32_t warning_reasons(const net::CookieInclusionStatus& s) {
    return static_cast<uint32_t>(s.warning_reasons().to_ulong());
  }
  static bool Read(network::mojom::CookieInclusionStatusDataView status,
                   net::CookieInclusionStatus* out);
};

template <>
struct StructTraits<network::mojom::CookieAndLineWithAccessResultDataView,
                    net::CookieAndLineWithAccessResult> {
  static const absl::optional<net::CanonicalCookie>& cookie(
      const net::CookieAndLineWithAccessResult& c) {
    return c.cookie;
  }
  static const std::string& cookie_string(
      const net::CookieAndLineWithAccessResult& c) {
    return c.cookie_string;
  }
  static const net::CookieAccessResult& access_result(
      const net::CookieAndLineWithAccessResult& c) {
    return c.access_result;
  }
  static bool Read(network::mojom::CookieAndLineWithAccessResultDataView cookie,
                   net::CookieAndLineWithAccessResult* out);
};

template <>
struct StructTraits<network::mojom::CookieAccessResultDataView,
                    net::CookieAccessResult> {
  static const net::CookieEffectiveSameSite& effective_same_site(
      const net::CookieAccessResult& c) {
    return c.effective_same_site;
  }
  static const net::CookieInclusionStatus& status(
      const net::CookieAccessResult& c) {
    return c.status;
  }
  static const net::CookieAccessSemantics& access_semantics(
      const net::CookieAccessResult& c) {
    return c.access_semantics;
  }
  static bool is_allowed_to_access_secure_cookies(
      const net::CookieAccessResult& c) {
    return c.is_allowed_to_access_secure_cookies;
  }
  static bool Read(network::mojom::CookieAccessResultDataView access_result,
                   net::CookieAccessResult* out);
};

template <>
struct StructTraits<network::mojom::CookieWithAccessResultDataView,
                    net::CookieWithAccessResult> {
  static const net::CanonicalCookie& cookie(
      const net::CookieWithAccessResult& c) {
    return c.cookie;
  }
  static const net::CookieAccessResult& access_result(
      const net::CookieWithAccessResult& c) {
    return c.access_result;
  }
  static bool Read(network::mojom::CookieWithAccessResultDataView cookie,
                   net::CookieWithAccessResult* out);
};

template <>
struct StructTraits<network::mojom::CookieChangeInfoDataView,
                    net::CookieChangeInfo> {
  static const net::CanonicalCookie& cookie(const net::CookieChangeInfo& c) {
    return c.cookie;
  }
  static const net::CookieAccessResult& access_result(
      const net::CookieChangeInfo& c) {
    return c.access_result;
  }
  static net::CookieChangeCause cause(const net::CookieChangeInfo& c) {
    return c.cause;
  }
  static bool Read(network::mojom::CookieChangeInfoDataView info,
                   net::CookieChangeInfo* out);
};

template <>
struct StructTraits<network::mojom::SamePartyContextDataView,
                    net::SamePartyContext> {
  static net::SamePartyContext::Type context_type(
      const net::SamePartyContext& s) {
    return s.context_type();
  }
  static net::SamePartyContext::Type ancestors_for_metrics_only(
      const net::SamePartyContext& s) {
    return s.ancestors_for_metrics_only();
  }
  static net::SamePartyContext::Type top_resource_for_metrics_only(
      const net::SamePartyContext& s) {
    return s.top_resource_for_metrics_only();
  }

  static bool Read(network::mojom::SamePartyContextDataView bundle,
                   net::SamePartyContext* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_COOKIE_MANAGER_MOJOM_TRAITS_H_
