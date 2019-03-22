// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

WebAppDataRetriever::WebAppDataRetriever() = default;

WebAppDataRetriever::~WebAppDataRetriever() = default;

void WebAppDataRetriever::GetWebApplicationInfo(
    content::WebContents* web_contents,
    GetWebApplicationInfoCallback callback) {
  // Concurrent calls are not allowed.
  CHECK(!get_web_app_info_callback_);
  get_web_app_info_callback_ = std::move(callback);

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(get_web_app_info_callback_), nullptr));
    return;
  }

  chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame;
  web_contents->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Set the error handler so that we can run |get_web_app_info_callback_| if
  // the WebContents or the RenderFrameHost are destroyed and the connection
  // to ChromeRenderFrame is lost.
  chrome_render_frame.set_connection_error_handler(
      base::BindOnce(&WebAppDataRetriever::OnGetWebApplicationInfoFailed,
                     weak_ptr_factory_.GetWeakPtr()));
  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* web_app_info_proxy = chrome_render_frame.get();
  web_app_info_proxy->GetWebApplicationInfo(base::BindOnce(
      &WebAppDataRetriever::OnGetWebApplicationInfo,
      weak_ptr_factory_.GetWeakPtr(), std::move(chrome_render_frame),
      web_contents, entry->GetUniqueID()));
}

void WebAppDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    CheckInstallabilityCallback callback) {
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents);
  DCHECK(installable_manager);

  // TODO(crbug.com/829232) Unify with other calls to GetData.
  InstallableParams params;
  params.check_eligibility = true;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.has_worker = true;
  // Do not wait_for_worker. OnDidPerformInstallableCheck is always invoked.
  installable_manager->GetData(
      params, base::BindRepeating(
                  &WebAppDataRetriever::OnDidPerformInstallableCheck,
                  weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void WebAppDataRetriever::GetIcons(content::WebContents* web_contents,
                                   const std::vector<GURL>& icon_urls,
                                   bool skip_page_fav_icons,
                                   GetIconsCallback callback) {
  DCHECK(!icon_urls.empty());

  // TODO(loyso): Refactor WebAppIconDownloader: crbug.com/907296.
  icon_downloader_ = std::make_unique<WebAppIconDownloader>(
      web_contents, icon_urls, "WebApp.Icon.HttpStatusCodeClassOnCreate",
      base::BindOnce(&WebAppDataRetriever::OnIconsDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  if (skip_page_fav_icons)
    icon_downloader_->SkipPageFavicons();

  icon_downloader_->Start();
}

void WebAppDataRetriever::OnGetWebApplicationInfo(
    chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame,
    content::WebContents* web_contents,
    int last_committed_nav_entry_unique_id,
    const WebApplicationInfo& web_app_info) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry || last_committed_nav_entry_unique_id != entry->GetUniqueID()) {
    std::move(get_web_app_info_callback_).Run(nullptr);
    return;
  }

  auto info = std::make_unique<WebApplicationInfo>(web_app_info);
  if (info->app_url.is_empty())
    info->app_url = web_contents->GetLastCommittedURL();

  if (info->title.empty())
    info->title = web_contents->GetTitle();
  if (info->title.empty())
    info->title = base::UTF8ToUTF16(info->app_url.spec());

  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnGetWebApplicationInfoFailed() {
  std::move(get_web_app_info_callback_).Run(nullptr);
}

void WebAppDataRetriever::OnDidPerformInstallableCheck(
    CheckInstallabilityCallback callback,
    const InstallableData& data) {
  DCHECK(data.manifest_url.is_valid() || data.manifest->IsEmpty());

  const bool is_installable = data.error_code == NO_ERROR_DETECTED;

  std::move(callback).Run(*data.manifest, is_installable);
}

void WebAppDataRetriever::OnIconsDownloaded(GetIconsCallback callback,
                                            bool success,
                                            const IconsMap& icons_map) {
  // |icons_map| is owned by |icon_downloader_|. Take a copy before destroying
  // the downloader. Return empty |result_map| if the tab has navigated away
  // during the icon download.
  IconsMap result_map;
  if (success)
    result_map = icons_map;
  icon_downloader_.reset();

  std::move(callback).Run(std::move(result_map));
}

}  // namespace web_app
