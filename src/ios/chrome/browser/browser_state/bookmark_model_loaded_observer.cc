// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/bookmark_model_loaded_observer.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"

BookmarkModelLoadedObserver::BookmarkModelLoadedObserver(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

void BookmarkModelLoadedObserver::BookmarkModelChanged() {}

void BookmarkModelLoadedObserver::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  // Causes lazy-load if sync is enabled.
  ProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  model->RemoveObserver(this);
  delete this;
}

void BookmarkModelLoadedObserver::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  model->RemoveObserver(this);
  delete this;
}
