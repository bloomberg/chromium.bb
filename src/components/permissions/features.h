// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_FEATURES_H_
#define COMPONENTS_PERMISSIONS_FEATURES_H_

#include "base/feature_list.h"

namespace permissions {
namespace features {

extern const base::Feature kBlockPromptsIfDismissedOften;
extern const base::Feature kBlockPromptsIfIgnoredOften;
extern const base::Feature kBlockRepeatedNotificationPermissionPrompts;
extern const base::Feature kPermissionDelegation;

}  // namespace features
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_FEATURES_H_
