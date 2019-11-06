// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view_util.h"

#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"

std::unique_ptr<views::View> CreateBubbleSyncPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    const BubbleSyncPromoViewParams& params) {
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return std::make_unique<DiceBubbleSyncPromoView>(
        profile, delegate, access_point,
        params.dice_no_accounts_promo_message_resource_id,
        params.dice_accounts_promo_message_resource_id,
        params.dice_signin_button_prominent, params.dice_text_style);
  } else {
    return std::make_unique<BubbleSyncPromoView>(
        delegate, access_point, params.link_text_resource_id,
        params.message_text_resource_id);
  }
}
