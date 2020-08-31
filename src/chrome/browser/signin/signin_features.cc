// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_features.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const base::Feature kDiceWebSigninInterceptionFeature{
    "DiceWebSigninInterception", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_DICE_SUPPORT
