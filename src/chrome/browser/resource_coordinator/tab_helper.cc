// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
// TODO(siggi): This is an abomination, remove this as soon as the page signal
//     receiver is abolished.
#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorTabHelper::ResourceCoordinatorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  TabLoadTracker::Get()->StartTracking(web_contents);

  // The invalid type is used as sentinel for no CU ID available.
  DCHECK_EQ(CoordinationUnitType::kInvalidType, page_cu_id_.type);
  bool have_cu_id = performance_manager::PerformanceManagerTabHelper::
      GetCoordinationIDForWebContents(web_contents, &page_cu_id_);

  // This can happen in unit tests.
  if (have_cu_id) {
    DCHECK_EQ(CoordinationUnitType::kPage, page_cu_id_.type);
    if (auto* page_signal_receiver = GetPageSignalReceiver()) {
      // Gets CoordinationUnitID for this WebContents and adds it to
      // PageSignalReceiver.
      page_signal_receiver->AssociateCoordinationUnitIDWithWebContents(
          page_cu_id_, web_contents);
    }
  } else {
    DCHECK_EQ(CoordinationUnitType::kInvalidType, page_cu_id_.type);
  }

  if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
    auto* rc_parts = g_browser_process->resource_coordinator_parts();
    DCHECK(rc_parts);
    rc_parts->tab_memory_metrics_reporter()->StartReporting(
        TabLoadTracker::Get());
  }

#if !defined(OS_ANDROID)
  // Don't create the LocalSiteCharacteristicsWebContentsObserver for this tab
  // we don't have a page signal receiver as the data that this observer
  // records depend on it.
  if (base::FeatureList::IsEnabled(features::kSiteCharacteristicsDatabase) &&
      GetPageSignalReceiver()) {
    local_site_characteristics_wc_observer_ =
        std::make_unique<LocalSiteCharacteristicsWebContentsObserver>(
            web_contents);
  }
#endif
}

ResourceCoordinatorTabHelper::~ResourceCoordinatorTabHelper() = default;

void ResourceCoordinatorTabHelper::DidStartLoading() {
  TabLoadTracker::Get()->DidStartLoading(web_contents());
}

void ResourceCoordinatorTabHelper::DidReceiveResponse() {
  TabLoadTracker::Get()->DidReceiveResponse(web_contents());
}

void ResourceCoordinatorTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  TabLoadTracker::Get()->DidFailLoad(web_contents());
}

void ResourceCoordinatorTabHelper::RenderProcessGone(
    base::TerminationStatus status) {
  // TODO(siggi): Looks like this can be acquired in a more timely manner from
  //    the RenderProcessHostObserver.
  TabLoadTracker::Get()->RenderProcessGone(web_contents(), status);
}

void ResourceCoordinatorTabHelper::WebContentsDestroyed() {
  if (page_cu_id_.type == CoordinationUnitType::kPage) {
    if (auto* page_signal_receiver = GetPageSignalReceiver()) {
      // Gets CoordinationUnitID for this WebContents and removes it from
      // PageSignalReceiver.
      page_signal_receiver->RemoveCoordinationUnitID(page_cu_id_);
    }
  }

  TabLoadTracker::Get()->StopTracking(web_contents());
}

void ResourceCoordinatorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (navigation_handle->IsInMainFrame()) {
    if (auto* page_signal_receiver = GetPageSignalReceiver()) {
      // Update the last observed navigation ID for this WebContents.
      page_signal_receiver->SetNavigationID(
          web_contents(), navigation_handle->GetNavigationId());
    }
  }

  if (navigation_handle->IsInMainFrame()) {
    ukm_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceCoordinatorTabHelper)

}  // namespace resource_coordinator
