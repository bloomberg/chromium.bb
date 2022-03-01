// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_test_helper.h"

#include <memory>

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"

namespace policy {

DlpContentManagerTestHelper::DlpContentManagerTestHelper() {
  manager_ = new DlpContentManager();
  DCHECK(manager_);
  reporting_manager_ = new DlpReportingManager();
  DCHECK(reporting_manager_);
  manager_->SetReportingManagerForTesting(reporting_manager_);
  DlpContentManager::SetDlpContentManagerForTesting(manager_);
}

DlpContentManagerTestHelper::~DlpContentManagerTestHelper() {
  delete reporting_manager_;
}

void DlpContentManagerTestHelper::ChangeConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restrictions) {
  DCHECK(manager_);
  manager_->OnConfidentialityChanged(web_contents, restrictions);
}

void DlpContentManagerTestHelper::ChangeVisibility(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnVisibilityChanged(web_contents);
}

void DlpContentManagerTestHelper::DestroyWebContents(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnWebContentsDestroyed(web_contents);
}

void DlpContentManagerTestHelper::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> notifier) {
  DCHECK(manager_);
  manager_->SetWarnNotifierForTesting(std::move(notifier));
}

void DlpContentManagerTestHelper::ResetWarnNotifierForTesting() {
  DCHECK(manager_);
  manager_->ResetWarnNotifierForTesting();
}

bool DlpContentManagerTestHelper::HasContentCachedForRestriction(
    content::WebContents* web_contents,
    DlpRulesManager::Restriction restriction) const {
  DCHECK(manager_);
  return manager_->user_allowed_contents_cache_.Contains(web_contents,
                                                         restriction);
}

bool DlpContentManagerTestHelper::HasAnyContentCached() const {
  DCHECK(manager_);
  return manager_->user_allowed_contents_cache_.GetSizeForTesting() != 0;
}

base::TimeDelta DlpContentManagerTestHelper::GetPrivacyScreenOffDelay() const {
  DCHECK(manager_);
  return manager_->GetPrivacyScreenOffDelayForTesting();
}

DlpContentManager* DlpContentManagerTestHelper::GetContentManager() const {
  return manager_;
}

DlpReportingManager* DlpContentManagerTestHelper::GetReportingManager() const {
  return manager_->reporting_manager_;
}

}  // namespace policy
