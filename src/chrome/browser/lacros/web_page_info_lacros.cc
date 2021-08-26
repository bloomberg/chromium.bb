// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/web_page_info_lacros.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "ui/aura/window.h"

namespace crosapi {
namespace {

mojom::WebPageInfoPtr PopulateWebPageInfoFromBrowser(const Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    return nullptr;

  const TabStripModel* const tab_strip_model = browser->tab_strip_model();
  DCHECK(tab_strip_model);

  content::WebContents* const contents =
      tab_strip_model->GetActiveWebContents();
  if (!contents)
    return nullptr;

  ukm::SourceId source_id = ukm::GetSourceIdForWebContentsDocument(contents);
  if (source_id == ukm::kInvalidSourceId)
    return nullptr;

  mojom::WebPageInfoPtr web_page_info = mojom::WebPageInfo::New();

  web_page_info->source_id = source_id;

  // Domain could be empty.
  web_page_info->domain = contents->GetLastCommittedURL().host();
  // Engagement score could be -1 if engagement service is disabled.
  web_page_info->engagement_score =
      TabMetricsLogger::GetSiteEngagementScore(contents);
  web_page_info->has_form_entry =
      FormInteractionTabHelper::FromWebContents(contents)
          ->had_form_interaction();

  return web_page_info;
}

// TODO(alanlxl): this method is mostly copied from
// UserActivityManager::UpdateOpenTabURL in
// src/chrome/browser/chromeos/power/ml/user_activity_manager.cc
// Better to get rid of the duplicated code.
mojom::WebPageInfoPtr UpdateOpenTabURL() {
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  // Find the active tab in the visible focused or topmost browser.
  for (auto browser_iterator =
           browser_list->begin_browsers_ordered_by_activation();
       browser_iterator != browser_list->end_browsers_ordered_by_activation();
       ++browser_iterator) {
    const Browser* browser = *browser_iterator;
    DCHECK(browser);

    const BrowserWindow* window = browser->window();
    DCHECK(window);

    // We only need the visible focused or topmost browser.
    if (!window->GetNativeWindow()->IsVisible())
      continue;

    return PopulateWebPageInfoFromBrowser(browser);
  }
  return nullptr;
}

}  // namespace

WebPageInfoProviderLacros::WebPageInfoProviderLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<mojom::WebPageInfoFactory>())
    return;
  service->GetRemote<mojom::WebPageInfoFactory>()->RegisterWebPageInfoProvider(
      receiver_.BindNewPipeAndPassRemote());
}

WebPageInfoProviderLacros::~WebPageInfoProviderLacros() = default;

void WebPageInfoProviderLacros::RequestCurrentWebPageInfo(
    RequestCurrentWebPageInfoCallback callback) {
  mojom::WebPageInfoPtr result = UpdateOpenTabURL();
  std::move(callback).Run(std::move(result));
}

}  // namespace crosapi
