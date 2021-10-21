// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

const char ContentAutofillDriverFactory::
    kContentAutofillDriverFactoryWebContentsUserDataKey[] =
        "web_contents_autofill_driver_factory";

// static
void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey,
      base::WrapUnique(new ContentAutofillDriverFactory(
          contents, client, app_locale, enable_download_manager,
          std::move(autofill_manager_factory_callback))));
}

// static
ContentAutofillDriverFactory* ContentAutofillDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentAutofillDriverFactory*>(contents->GetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey));
}

// static
void ContentAutofillDriverFactory::BindAutofillDriver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  ContentAutofillDriverFactory* factory = FromWebContents(web_contents);
  if (!factory) {
    // The message pipe will be closed and raise a connection error to peer
    // side. The peer side can reconnect later when needed.
    return;
  }

  if (auto* driver = factory->DriverForFrame(render_frame_host))
    driver->BindPendingReceiver(std::move(pending_receiver));
}

ContentAutofillDriverFactory::ContentAutofillDriverFactory(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback)
    : content::WebContentsObserver(web_contents),
      client_(client),
      app_locale_(app_locale),
      enable_download_manager_(enable_download_manager),
      autofill_manager_factory_callback_(
          std::move(autofill_manager_factory_callback)) {}

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() = default;

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto insertion_result = driver_map_.emplace(render_frame_host, nullptr);
  std::unique_ptr<ContentAutofillDriver>& driver =
      insertion_result.first->second;
  bool insertion_happened = insertion_result.second;
  if (insertion_happened) {
    // The `render_frame_host` may already be deleted (or be in the process of
    // being deleted). In this case, we must not create a new driver. Otherwise,
    // a driver might hold a deallocated RFH.
    //
    // For example, `render_frame_host` is deleted in the following sequence:
    // 1. `render_frame_host->~RenderFrameHostImpl()` starts and marks
    //    `render_frame_host` as deleted.
    // 2. `ContentAutofillDriverFactory::RenderFrameDeleted(render_frame_host)`
    //    destroys the driver of `render_frame_host`.
    // 3. `SomeOtherWebContentsObserver::RenderFrameDeleted(render_frame_host)`
    //    calls `DriverForFrame(render_frame_host)`.
    // 5. `render_frame_host->~RenderFrameHostImpl()` finishes.
    if (render_frame_host->IsRenderFrameCreated()) {
      driver = std::make_unique<ContentAutofillDriver>(
          render_frame_host, client(), app_locale_, &router_,
          enable_download_manager_, autofill_manager_factory_callback_);
      DCHECK_EQ(driver_map_.find(render_frame_host)->second.get(),
                driver.get());
    } else {
      driver_map_.erase(insertion_result.first);
      DCHECK_EQ(driver_map_.count(render_frame_host), 0u);
      return nullptr;
    }
  }
  DCHECK(driver.get());
  return driver.get();
}

void ContentAutofillDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = driver_map_.find(render_frame_host);
  if (it == driver_map_.end())
    return;

  ContentAutofillDriver* driver = it->second.get();
  DCHECK(driver);

  if (render_frame_host->GetLifecycleState() !=
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    driver->MaybeReportAutofillWebOTPMetrics();
  }

  // If the popup menu has been triggered from within an iframe and that
  // frame is deleted, hide the popup. This is necessary because the popup
  // may actually be shown by the AutofillExternalDelegate of an ancestor
  // frame, which is not notified about |render_frame_host|'s destruction
  // and therefore won't close the popup.
  if (render_frame_host->GetParent() &&
      router_.last_queried_source() == driver) {
    DCHECK_NE(content::RenderFrameHost::LifecycleState::kPrerendering,
              render_frame_host->GetLifecycleState());
    router_.HidePopup(driver);
  }

  driver_map_.erase(it);
}

void ContentAutofillDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug/1117451): Clean up experiment code.
  if (base::FeatureList::IsEnabled(
          features::kAutofillProbableFormSubmissionInBrowser) &&
      navigation_handle->IsRendererInitiated() &&
      !navigation_handle->WasInitiatedByLinkClick() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    content::GlobalRenderFrameHostId id =
        navigation_handle->GetPreviousRenderFrameHostId();
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(id);
    if (render_frame_host) {
      if (auto* driver = DriverForFrame(render_frame_host))
        driver->ProbablyFormSubmitted();
    }
  }
}

void ContentAutofillDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      (navigation_handle->IsInMainFrame() ||
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    if (auto* driver =
            DriverForFrame(navigation_handle->GetRenderFrameHost())) {
      if (!navigation_handle->IsInPrerenderedMainFrame())
        client_->HideAutofillPopup(PopupHidingReason::kNavigation);
      driver->DidNavigateFrame(navigation_handle);
    }
  }
}

void ContentAutofillDriverFactory::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    client_->HideAutofillPopup(PopupHidingReason::kTabGone);
}

void ContentAutofillDriverFactory::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  content::GlobalRenderFrameHostId render_frame_host_id(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());
  // No need to report the metrics here if navigating to a different
  // RenderFrameHost. It will be reported in |RenderFrameDeleted|.
  // TODO(crbug.com/936696): Remove this logic when RenderDocument is enabled
  // everywhere.
  if (render_frame_host_id !=
      navigation_handle->GetPreviousRenderFrameHostId()) {
    return;
  }
  // Do not report metrics if prerendering.
  if (render_frame_host->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    return;
  }
  if (auto* driver = DriverForFrame(render_frame_host))
    driver->MaybeReportAutofillWebOTPMetrics();
}

}  // namespace autofill
