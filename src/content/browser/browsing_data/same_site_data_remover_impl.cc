// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/same_site_data_remover_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/same_site_data_remover.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

const uint32_t kStoragePartitionRemovalMask =
    content::StoragePartition::REMOVE_DATA_MASK_ALL &
    ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;

void OnGetAllCookiesWithAccessSemantics(
    base::OnceClosure closure,
    network::mojom::CookieManager* cookie_manager,
    std::set<std::string>* same_site_none_domains,
    const std::vector<net::CanonicalCookie>& cookies,
    const std::vector<net::CookieAccessSemantics>& access_semantics_list) {
  DCHECK(cookies.size() == access_semantics_list.size());
  base::RepeatingClosure barrier =
      base::BarrierClosure(cookies.size(), std::move(closure));
  for (size_t i = 0; i < cookies.size(); ++i) {
    const net::CanonicalCookie& cookie = cookies[i];
    if (cookie.IsEffectivelySameSiteNone(access_semantics_list[i])) {
      same_site_none_domains->emplace(cookie.Domain());
      cookie_manager->DeleteCanonicalCookie(
          cookie, base::BindOnce([](const base::RepeatingClosure& callback,
                                    bool success) { callback.Run(); },
                                 barrier));
    } else {
      barrier.Run();
    }
  }
}

bool DoesOriginMatchDomain(const std::set<std::string>& same_site_none_domains,
                           const url::Origin& origin,
                           storage::SpecialStoragePolicy* policy) {
  for (const std::string& domain : same_site_none_domains) {
    if (net::cookie_util::IsDomainMatch(domain, origin.host())) {
      return true;
    }
  }
  return false;
}

}  // namespace

SameSiteDataRemoverImpl::SameSiteDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      storage_partition_(browser_context_->GetDefaultStoragePartition()) {
  DCHECK(browser_context_);
}

SameSiteDataRemoverImpl::~SameSiteDataRemoverImpl() {}

const std::set<std::string>&
SameSiteDataRemoverImpl::GetDeletedDomainsForTesting() {
  return same_site_none_domains_;
}

void SameSiteDataRemoverImpl::OverrideStoragePartitionForTesting(
    StoragePartition* storage_partition) {
  storage_partition_ = storage_partition;
}

void SameSiteDataRemoverImpl::DeleteSameSiteNoneCookies(
    base::OnceClosure closure) {
  same_site_none_domains_.clear();
  auto* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookiesWithAccessSemantics(
      base::BindOnce(&OnGetAllCookiesWithAccessSemantics, std::move(closure),
                     cookie_manager, &same_site_none_domains_));
}

void SameSiteDataRemoverImpl::ClearStoragePartitionData(
    base::OnceClosure closure) {
  // TODO(crbug.com/987177): Figure out how to handle protected storage.
  storage_partition_->ClearData(
      kStoragePartitionRemovalMask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      base::BindRepeating(&DoesOriginMatchDomain, same_site_none_domains_),
      nullptr, false, base::Time(), base::Time::Max(), std::move(closure));
}

void SameSiteDataRemoverImpl::ClearStoragePartitionForOrigins(
    base::OnceClosure closure,
    StoragePartition::OriginMatcherFunction origin_matcher) {
  // TODO(crbug.com/987177): Figure out how to handle protected storage.
  storage_partition_->ClearData(
      kStoragePartitionRemovalMask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      std::move(origin_matcher), nullptr, false, base::Time(),
      base::Time::Max(), std::move(closure));
}

// Defines the ClearSameSiteNoneData function declared in same_site_remover.h.
// Clears cookies and associated data available in third-party contexts.
void ClearSameSiteNoneData(base::OnceClosure closure, BrowserContext* context) {
  auto same_site_remover = std::make_unique<SameSiteDataRemoverImpl>(context);
  SameSiteDataRemoverImpl* remover = same_site_remover.get();

  remover->DeleteSameSiteNoneCookies(
      base::BindOnce(&SameSiteDataRemoverImpl::ClearStoragePartitionData,
                     std::move(same_site_remover), std::move(closure)));
}

void ClearSameSiteNoneCookiesAndStorageForOrigins(
    base::OnceClosure closure,
    BrowserContext* context,
    StoragePartition::OriginMatcherFunction clear_storage_origin_matcher) {
  auto same_site_remover = std::make_unique<SameSiteDataRemoverImpl>(context);
  SameSiteDataRemoverImpl* remover = same_site_remover.get();

  remover->DeleteSameSiteNoneCookies(
      base::BindOnce(&SameSiteDataRemoverImpl::ClearStoragePartitionForOrigins,
                     std::move(same_site_remover), std::move(closure),
                     std::move(clear_storage_origin_matcher)));
}

}  // namespace content
