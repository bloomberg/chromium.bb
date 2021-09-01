// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONFIRMATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONFIRMATION_MANAGER_H_

#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"

class NearbySharingService;

class NearbyConfirmationManager
    : public nearby_share::mojom::ConfirmationManager {
 public:
  NearbyConfirmationManager(NearbySharingService* nearby_service,
                            ShareTarget share_target);
  NearbyConfirmationManager(const NearbyConfirmationManager&) = delete;
  NearbyConfirmationManager& operator=(const NearbyConfirmationManager&) =
      delete;
  ~NearbyConfirmationManager() override;

  // nearby_share::mojom::ConfirmationManager:
  void Accept(AcceptCallback callback) override;
  void Reject(RejectCallback callback) override;
  void Cancel(CancelCallback callback) override;

 private:
  NearbySharingService* nearby_service_;
  ShareTarget share_target_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONFIRMATION_MANAGER_H_
