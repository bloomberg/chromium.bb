// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data/browsing_data_remover.h"

#import "ios/web/browsing_data/browsing_data_remover_observer.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kWebBrowsingDataRemoverKeyName[] = "web_browsing_data_remover";
}  // namespace

namespace web {

BrowsingDataRemover::BrowsingDataRemover(web::BrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
  observers_list_ = [NSHashTable weakObjectsHashTable];
}

BrowsingDataRemover::~BrowsingDataRemover() {}

// static
BrowsingDataRemover* BrowsingDataRemover::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  BrowsingDataRemover* browsing_data_remover =
      static_cast<BrowsingDataRemover*>(
          browser_state->GetUserData(kWebBrowsingDataRemoverKeyName));
  if (!browsing_data_remover) {
    browsing_data_remover = new BrowsingDataRemover(browser_state);
    browser_state->SetUserData(kWebBrowsingDataRemoverKeyName,
                               base::WrapUnique(browsing_data_remover));
  }
  return browsing_data_remover;
}

void BrowsingDataRemover::ClearBrowsingData(ClearBrowsingDataMask types) {
  // TODO(crbug.com/619783):implement this.
}

void BrowsingDataRemover::AddObserver(
    id<BrowsingDataRemoverObserver> observer) {
  [observers_list_ addObject:observer];
}

void BrowsingDataRemover::RemoveObserver(
    id<BrowsingDataRemoverObserver> observer) {
  [observers_list_ removeObject:observer];
}

}  // namespace web
