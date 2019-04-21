// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider_impl.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace previews {

PreviewsTopHostProviderImpl::PreviewsTopHostProviderImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

PreviewsTopHostProviderImpl::~PreviewsTopHostProviderImpl() {}

std::vector<std::string> PreviewsTopHostProviderImpl::GetTopHosts(
    size_t max_sites) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);

  std::vector<std::string> top_hosts;
  top_hosts.reserve(max_sites);

  // Create SiteEngagementService to request site engagement scores.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included.
  //
  // TODO(mcrouse): Filter to only include Top hosts since Data Saver was
  // enabled.
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();
  for (const auto& detail : engagement_details) {
    if (top_hosts.size() <= max_sites) {
      if (detail.origin.SchemeIs(url::kHttpsScheme)) {
        top_hosts.push_back(detail.origin.host());
      }
    } else {
      break;
    }
  }

  return top_hosts;
}

}  // namespace previews
