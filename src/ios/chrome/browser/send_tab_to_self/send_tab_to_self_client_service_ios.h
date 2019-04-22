// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace ios {
class ChromeBrowserState;
}

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
class SendTabToSelfClientServiceIOS : public KeyedService,
                                      public SendTabToSelfModelObserver {
 public:
  SendTabToSelfClientServiceIOS(ios::ChromeBrowserState* browser_state,
                                SendTabToSelfModel* model);
  ~SendTabToSelfClientServiceIOS() override;

  // Keeps track of when the model is loaded so that updates to the
  // model can be pushed afterwards.
  void SendTabToSelfModelLoaded() override;
  // Updates the UI to reflect the new entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  // Updates the UI to reflect the removal of entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override;

 private:
  // Owned by the SendTabToSelfSyncService which should outlive this class
  SendTabToSelfModel* model_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfClientServiceIOS);
};

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_
