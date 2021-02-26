// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_ui_service.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "url/gurl.h"

namespace previews {

PreviewsUIService::PreviewsUIService(
    std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
    std::unique_ptr<blocklist::OptOutStore> previews_opt_out_store,
    std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
    const PreviewsIsEnabledCallback& is_enabled_callback,
    std::unique_ptr<PreviewsLogger> logger,
    blocklist::BlocklistData::AllowedTypesAndVersions allowed_previews,
    network::NetworkQualityTracker* network_quality_tracker)
    : previews_decider_impl_(std::move(previews_decider_impl)),
      logger_(std::move(logger)),
      network_quality_tracker_(network_quality_tracker) {
  DCHECK(logger_);
  DCHECK(previews_decider_impl_);
  DCHECK(network_quality_tracker_);
  previews_decider_impl_->Initialize(
      this, std::move(previews_opt_out_store), std::move(previews_opt_guide),
      is_enabled_callback, std::move(allowed_previews));
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
}

PreviewsUIService::~PreviewsUIService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
}

void PreviewsUIService::AddPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->AddPreviewNavigation(url, opt_out, type, page_id);
}

void PreviewsUIService::LogPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             base::Time time,
                                             uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogPreviewNavigation(url, type, opt_out, time, page_id);
}

void PreviewsUIService::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons,
    uint64_t page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogPreviewDecisionMade(reason, url, time, type,
                                  std::move(passed_reasons), page_id);
}

void PreviewsUIService::OnNewBlocklistedHost(const std::string& host,
                                             base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnNewBlocklistedHost(host, time);
}

void PreviewsUIService::OnUserBlocklistedStatusChange(bool blocklisted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnUserBlocklistedStatusChange(blocklisted);
}

void PreviewsUIService::OnBlocklistCleared(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnBlocklistCleared(time);
}

void PreviewsUIService::SetIgnorePreviewsBlocklistDecision(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->SetIgnorePreviewsBlocklistDecision(ignored);
}

void PreviewsUIService::OnIgnoreBlocklistDecisionStatusChanged(bool ignored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->OnIgnoreBlocklistDecisionStatusChanged(ignored);
}

PreviewsLogger* PreviewsUIService::previews_logger() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return logger_.get();
}

PreviewsDeciderImpl* PreviewsUIService::previews_decider_impl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return previews_decider_impl_.get();
}

void PreviewsUIService::ClearBlockList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previews_decider_impl_->ClearBlockList(begin_time, end_time);
}

void PreviewsUIService::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_effective_connection_type_ = type;
  previews_decider_impl_->SetEffectiveConnectionType(type);
}

}  // namespace previews
