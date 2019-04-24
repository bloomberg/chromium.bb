// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_FEATURES_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Used to control the state of the Credential Manager API feature.
extern const base::Feature kCredentialManager;

// Used to control the state of the iOS password generation feature.
extern const base::Feature kPasswordGeneration;

// Whether password generation is enabled.
bool IsAutomaticPasswordGenerationEnabled();

}  // namespace features

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_FEATURES_H_
