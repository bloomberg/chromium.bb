// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/features.h"

namespace permissions {
namespace features {

// Enables or disables whether permission prompts are automatically blocked
// after the user has explicitly dismissed them too many times.
const base::Feature kBlockPromptsIfDismissedOften{
    "BlockPromptsIfDismissedOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables whether permission prompts are automatically blocked
// after the user has ignored them too many times.
const base::Feature kBlockPromptsIfIgnoredOften{
    "BlockPromptsIfIgnoredOften", base::FEATURE_ENABLED_BY_DEFAULT};

// Once the user declines a notification permission prompt in a WebContents,
// automatically dismiss subsequent prompts in the same WebContents, from any
// origin, until the next user-initiated navigation.
const base::Feature kBlockRepeatedNotificationPermissionPrompts{
    "BlockRepeatedNotificationPermissionPrompts",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Delegate permissions to cross-origin iframes when the feature has been
// allowed by feature policy.
const base::Feature kPermissionDelegation{"PermissionDelegation",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace permissions
