// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/about_this_site_service.h"

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace page_info {
using AboutThisSiteStatus = about_this_site_validation::AboutThisSiteStatus;
using OptimizationGuideDecision = optimization_guide::OptimizationGuideDecision;

AboutThisSiteService::AboutThisSiteService(std::unique_ptr<Client> client)
    : client_(std::move(client)) {}

absl::optional<proto::SiteInfo> AboutThisSiteService::GetAboutThisSiteInfo(
    const GURL& url,
    ukm::SourceId source_id) const {
  optimization_guide::OptimizationMetadata metadata;
  auto decision = client_->CanApplyOptimization(url, &metadata);
  absl::optional<proto::AboutThisSiteMetadata> about_this_site_metadata =
      metadata.ParsedMetadata<proto::AboutThisSiteMetadata>();

  AboutThisSiteStatus status =
      decision == OptimizationGuideDecision::kUnknown
          ? AboutThisSiteStatus::kUnknown
          : about_this_site_validation::ValidateMetadata(
                about_this_site_metadata);
  base::UmaHistogramEnumeration("Security.PageInfo.AboutThisSiteStatus",
                                status);
  ukm::builders::AboutThisSiteStatus(source_id)
      .SetStatus(static_cast<int>(status))
      .Record(ukm::UkmRecorder::Get());
  if (status == AboutThisSiteStatus::kValid) {
    return about_this_site_metadata->site_info();
  }

  // TODO(crbug.com/1250653): Remove returning fake data after server-side is
  // ready.
  if (kShowSampleContent.Get()) {
    page_info::proto::SiteInfo site_info;
    if (url == GURL("https://example.com")) {
      auto* description = site_info.mutable_description();
      description->set_description(
          "A domain used in illustrative examples in documents.");
      description->mutable_source()->set_url("https://example.com");
      description->mutable_source()->set_label("Example source");
      return site_info;
    }

    if (url == GURL("https://permission.site")) {
      auto* description = site_info.mutable_description();
      description->set_description(
          "A site containing test buttons for various browser APIs, in order"
          " to trigger permission dialogues and similar UI in modern "
          "browsers.");
      description->mutable_source()->set_url("https://permission.site.com");
      description->mutable_source()->set_label("Permission Site");
      return site_info;
    }
  }

  return absl::nullopt;
}

AboutThisSiteService::~AboutThisSiteService() = default;

}  // namespace page_info
