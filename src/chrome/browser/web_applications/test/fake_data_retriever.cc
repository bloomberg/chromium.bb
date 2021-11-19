// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_data_retriever.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

FakeDataRetriever::FakeDataRetriever() = default;

FakeDataRetriever::~FakeDataRetriever() {
  if (destruction_callback_)
    std::move(destruction_callback_).Run();
}

void FakeDataRetriever::GetWebApplicationInfo(
    content::WebContents* web_contents,
    GetWebApplicationInfoCallback callback) {
  DCHECK(web_contents);

  completion_callback_ =
      base::BindOnce(std::move(callback), std::move(web_app_info_));
  ScheduleCompletionCallback();
}

void FakeDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    bool bypass_service_worker_check,
    CheckInstallabilityCallback callback) {
  completion_callback_ =
      base::BindOnce(std::move(callback), manifest_.Clone(), manifest_url_,
                     /*valid_manifest_for_web_app=*/true, is_installable_);
  ScheduleCompletionCallback();
}

void FakeDataRetriever::GetIcons(content::WebContents* web_contents,
                                 const std::vector<GURL>& icon_urls,
                                 bool skip_page_favicons,
                                 GetIconsCallback callback) {
  if (get_icons_delegate_) {
    icons_map_ =
        get_icons_delegate_.Run(web_contents, icon_urls, skip_page_favicons);
  }

  completion_callback_ =
      base::BindOnce(std::move(callback), icons_downloaded_result_,
                     std::move(icons_map_), std::move(icons_http_results_));
  ScheduleCompletionCallback();

  icons_map_.clear();
  icons_http_results_.clear();
}

void FakeDataRetriever::SetRendererWebApplicationInfo(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  web_app_info_ = std::move(web_app_info);
}

void FakeDataRetriever::SetEmptyRendererWebApplicationInfo() {
  SetRendererWebApplicationInfo(std::make_unique<WebApplicationInfo>());
}

void FakeDataRetriever::SetManifest(blink::mojom::ManifestPtr manifest,
                                    bool is_installable,
                                    GURL manifest_url) {
  manifest_ = std::move(manifest);
  is_installable_ = is_installable;
  manifest_url_ = manifest_url;
}

void FakeDataRetriever::SetIcons(IconsMap icons_map) {
  DCHECK(!get_icons_delegate_);
  icons_map_ = std::move(icons_map);
}

void FakeDataRetriever::SetGetIconsDelegate(
    GetIconsDelegate get_icons_delegate) {
  DCHECK(icons_map_.empty());
  get_icons_delegate_ = std::move(get_icons_delegate);
}

void FakeDataRetriever::SetIconsDownloadedResult(IconsDownloadedResult result) {
  icons_downloaded_result_ = result;
}

void FakeDataRetriever::SetDownloadedIconsHttpResults(
    DownloadedIconsHttpResults icons_http_results) {
  icons_http_results_ = std::move(icons_http_results);
}

void FakeDataRetriever::SetDestructionCallback(base::OnceClosure callback) {
  destruction_callback_ = std::move(callback);
}

void FakeDataRetriever::BuildDefaultDataToRetrieve(const GURL& url,
                                                   const GURL& scope) {
  SetEmptyRendererWebApplicationInfo();

  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = url;
  manifest->scope = scope;
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"Manifest Name";

  SetManifest(std::move(manifest), /*is_installable=*/true);

  SetIcons(IconsMap{});
}

void FakeDataRetriever::ScheduleCompletionCallback() {
  // If |this| DataRetriever destroyed, the completion callback gets cancelled.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDataRetriever::CallCompletionCallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeDataRetriever::CallCompletionCallback() {
  std::move(completion_callback_).Run();
}

}  // namespace web_app
