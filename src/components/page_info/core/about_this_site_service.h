// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace page_info {
namespace proto {
class SiteInfo;
}

// Provides "About this site" information for a web site. It includes short
// description about the website (from external source, usually from Wikipedia),
// when the website was first indexed and other data if available.
class AboutThisSiteService : public KeyedService {
 public:
  // Provides platform-independant access to an optimization guide service.
  // OptimizationGuideService on iOS doesn't implement OptimizationGuideDecider,
  // therefore the interface cannot be used in this service.
  class Client {
   public:
    virtual optimization_guide::OptimizationGuideDecision CanApplyOptimization(
        const GURL& url,
        optimization_guide::OptimizationMetadata* optimization_metadata) = 0;
    virtual ~Client() = default;
  };

  explicit AboutThisSiteService(std::unique_ptr<Client> client);
  ~AboutThisSiteService() override;

  AboutThisSiteService(const AboutThisSiteService&) = delete;
  AboutThisSiteService& operator=(const AboutThisSiteService&) = delete;

  // Returns "About this site" information for the website with |url|.
  absl::optional<proto::SiteInfo> GetAboutThisSiteInfo(
      const GURL& url,
      ukm::SourceId source_id) const;

  bool CanShowBanner(GURL url);
  void OnBannerDismissed(GURL url, ukm::SourceId source_id);
  void OnBannerURLOpened(GURL url, ukm::SourceId source_id);

  base::WeakPtr<AboutThisSiteService> GetWeakPtr();

 private:
  std::unique_ptr<Client> client_;
  base::flat_set<url::Origin> dismissed_banners_;

  base::WeakPtrFactory<AboutThisSiteService> weak_ptr_factory_{this};
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_SERVICE_H_
