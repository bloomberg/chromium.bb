// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/web_applications/web_app_origin_association_task.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_service.h"
#include "url/gurl.h"

namespace web_app {

WebAppOriginAssociationManager::WebAppOriginAssociationManager()
    : fetcher_(std::make_unique<webapps::WebAppOriginAssociationFetcher>()) {}

WebAppOriginAssociationManager::~WebAppOriginAssociationManager() = default;

void WebAppOriginAssociationManager::GetWebAppOriginAssociations(
    const GURL& manifest_url,
    apps::UrlHandlers url_handlers,
    OnDidGetWebAppOriginAssociations callback) {
  if (url_handlers.empty()) {
    std::move(callback).Run(apps::UrlHandlers());
    return;
  }

  auto task = std::make_unique<Task>(manifest_url, std::move(url_handlers),
                                     *this, std::move(callback));
  pending_tasks_.push_back(std::move(task));
  MaybeStartNextTask();
}

void WebAppOriginAssociationManager::MaybeStartNextTask() {
  if (task_in_progress_ || pending_tasks_.empty())
    return;

  task_in_progress_ = true;
  pending_tasks_.front().get()->Start();
}

void WebAppOriginAssociationManager::OnTaskCompleted() {
  DCHECK(!pending_tasks_.empty());
  task_in_progress_ = false;
  pending_tasks_.pop_front();
  MaybeStartNextTask();
}

void WebAppOriginAssociationManager::SetFetcherForTest(
    std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher) {
  fetcher_ = std::move(fetcher);
}

const mojo::Remote<webapps::mojom::WebAppOriginAssociationParser>&
WebAppOriginAssociationManager::GetParser() {
  if (!parser_ || !parser_.is_bound()) {
    parser_ = webapps::LaunchWebAppOriginAssociationParser();
    parser_.reset_on_disconnect();
  }

  return parser_;
}

webapps::WebAppOriginAssociationFetcher&
WebAppOriginAssociationManager::GetFetcher() {
  return *(fetcher_.get());
}

}  // namespace web_app
