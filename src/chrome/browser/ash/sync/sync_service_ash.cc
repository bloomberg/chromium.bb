// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_service_ash.h"

#include "base/feature_list.h"
#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"
#include "chrome/browser/ash/sync/sync_user_settings_client_ash.h"
#include "components/sync/base/features.h"

namespace ash {

SyncServiceAsh::SyncServiceAsh(syncer::SyncService* sync_service) {
  if (base::FeatureList::IsEnabled(
          syncer::kSyncChromeOSExplicitPassphraseSharing)) {
    explicit_passphrase_client_ =
        std::make_unique<SyncExplicitPassphraseClientAsh>(sync_service);
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    user_settings_client_ =
        std::make_unique<SyncUserSettingsClientAsh>(sync_service);
  }
}

SyncServiceAsh::~SyncServiceAsh() = default;

void SyncServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncServiceAsh::Shutdown() {
  receivers_.Clear();
  user_settings_client_ = nullptr;
  explicit_passphrase_client_ = nullptr;
}

void SyncServiceAsh::BindExplicitPassphraseClient(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  // Null if feature is disabled.
  if (explicit_passphrase_client_) {
    explicit_passphrase_client_->BindReceiver(std::move(receiver));
  }
}

void SyncServiceAsh::BindUserSettingsClient(
    mojo::PendingReceiver<crosapi::mojom::SyncUserSettingsClient> receiver) {
  // Null if feature is disabled.
  if (user_settings_client_) {
    user_settings_client_->BindReceiver(std::move(receiver));
  }
}

}  // namespace ash
