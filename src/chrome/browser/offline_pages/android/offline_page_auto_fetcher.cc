// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"

#include <utility>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

TabAndroid* FindTab(content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return nullptr;
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  if (!tab_model)
    return nullptr;
  // For this use-case, it's OK to fail if the active tab doesn't match
  // web_contents.
  if (tab_model->GetActiveWebContents() != web_contents)
    return nullptr;

  return tab_model->GetTabAt(tab_model->GetActiveIndex());
}

}  // namespace

OfflinePageAutoFetcher::OfflinePageAutoFetcher(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

OfflinePageAutoFetcher::~OfflinePageAutoFetcher() = default;

void OfflinePageAutoFetcher::TrySchedule(bool user_requested,
                                         TryScheduleCallback callback) {
  TabAndroid* tab = FindTab(render_frame_host_);
  if (!tab) {
    std::move(callback).Run(OfflinePageAutoFetcherScheduleResult::kOtherError);
    return;
  }
  GetService()->TrySchedule(user_requested,
                            render_frame_host_->GetLastCommittedURL(),
                            tab->GetAndroidId(), std::move(callback));
}

void OfflinePageAutoFetcher::CancelSchedule() {
  GetService()->CancelSchedule(render_frame_host_->GetLastCommittedURL());
}

// static
void OfflinePageAutoFetcher::Create(
    chrome::mojom::OfflinePageAutoFetcherRequest request,
    content::RenderFrameHost* render_frame_host) {
  mojo::MakeStrongBinding(
      std::make_unique<OfflinePageAutoFetcher>(render_frame_host),
      std::move(request));
}

OfflinePageAutoFetcherService* OfflinePageAutoFetcher::GetService() {
  return OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
      render_frame_host_->GetProcess()->GetBrowserContext());
}

}  // namespace offline_pages
