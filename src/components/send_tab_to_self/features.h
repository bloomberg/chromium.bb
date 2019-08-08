// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"

class PrefService;

namespace send_tab_to_self {

// If this feature is enabled, we will display the UI to send tabs if the Sync
// datatype is also enabled.
extern const base::Feature kSendTabToSelfShowSendingUI;

// If this feature is enabled, the tabs will be broadcasted instead of
// targeted to a specific device. This only affects the receiving side.
extern const base::Feature kSendTabToSelfBroadcast;

// Returns whether the receiving components of the feature is enabled on this
// device. This is different from IsReceivingEnabled in SendTabToSelfUtil
// because it doesn't rely on the SendTabToSelfSyncService to be actively up and
// ready.
bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
