// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_ios.h"

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace send_tab_to_self {

SendTabToSelfClientServiceIOS::SendTabToSelfClientServiceIOS(
    ios::ChromeBrowserState* browser_state,
    SendTabToSelfModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

SendTabToSelfClientServiceIOS::~SendTabToSelfClientServiceIOS() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void SendTabToSelfClientServiceIOS::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/949756): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientServiceIOS::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  if (!base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf)) {
    return;
  }
  NOTIMPLEMENTED();
}

void SendTabToSelfClientServiceIOS::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  if (!base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf)) {
    return;
  }
  NOTIMPLEMENTED();
}

}  // namespace send_tab_to_self
