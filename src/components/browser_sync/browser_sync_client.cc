// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser_sync_client.h"

#include "components/sync/model/model_type_store_service.h"

namespace browser_sync {

BrowserSyncClient::BrowserSyncClient() = default;

BrowserSyncClient::~BrowserSyncClient() = default;

base::FilePath BrowserSyncClient::GetSyncDataPath() {
  return GetModelTypeStoreService()->GetSyncDataPath();
}

}  // namespace browser_sync
