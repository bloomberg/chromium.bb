// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/url_list_manager.h"

UrlListManager::UrlListManager() = default;

UrlListManager::~UrlListManager() = default;

void UrlListManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UrlListManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UrlListManager::NotifyObservers(int32_t id, const GURL& url) {
  for (auto& observer : observers_) {
    observer.BlockedUrlAdded(id, url);
  }
}
