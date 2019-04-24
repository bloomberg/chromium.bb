// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

class Profile;

namespace send_tab_to_self {
class ReceivingUiHandlerRegistry;
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Singleton that owns all SendTabToSelfClientServices and associates them with
// Profile.
class SendTabToSelfClientService : public KeyedService,
                                   public SendTabToSelfModelObserver {
 public:
  SendTabToSelfClientService(Profile* profile, SendTabToSelfModel* model);

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

 protected:
  ~SendTabToSelfClientService() override;

 private:
  // Owned by the SendTabToSelfSyncService which should outlive this class
  SendTabToSelfModel* model_;
  // Singleton instance not owned by this class
  ReceivingUiHandlerRegistry* registry_;
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfClientService);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_H_
