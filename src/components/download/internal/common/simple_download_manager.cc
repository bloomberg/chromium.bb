// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/simple_download_manager.h"

namespace download {

SimpleDownloadManager::SimpleDownloadManager() {}

SimpleDownloadManager::~SimpleDownloadManager() {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnManagerGoingDown();
}

void SimpleDownloadManager::AddObserver(Observer* observer) {
  simple_download_manager_observers_.AddObserver(observer);
}

void SimpleDownloadManager::RemoveObserver(Observer* observer) {
  simple_download_manager_observers_.RemoveObserver(observer);
}

void SimpleDownloadManager::OnInitialized() {
  initialized_ = true;
  for (auto& callback : std::move(on_initialized_callbacks_))
    std::move(callback).Run();
}

void SimpleDownloadManager::NotifyWhenInitialized(
    base::OnceClosure on_initialized_cb) {
  if (initialized_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_initialized_cb));
    return;
  }
  on_initialized_callbacks_.emplace_back(std::move(on_initialized_cb));
}

void SimpleDownloadManager::OnNewDownloadCreated(DownloadItem* download) {
  for (auto& observer : simple_download_manager_observers_)
    observer.OnDownloadCreated(download);
}

}  // namespace download
