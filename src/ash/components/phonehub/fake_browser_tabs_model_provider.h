// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_MODEL_PROVIDER_H_
#define ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_MODEL_PROVIDER_H_

#include "ash/components/phonehub/browser_tabs_model_provider.h"

#include "ash/components/phonehub/browser_tabs_model.h"

namespace ash {
namespace phonehub {

class FakeBrowserTabsModelProvider : public BrowserTabsModelProvider {
 public:
  FakeBrowserTabsModelProvider();
  ~FakeBrowserTabsModelProvider() override;

  // BrowserTabsModelProvider:
  void TriggerRefresh() override {}

  void NotifyBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<BrowserTabsModel::BrowserTabMetadata>
          browser_tabs_metadata);
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_FAKE_BROWSER_TABS_MODEL_PROVIDER_H_
