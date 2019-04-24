// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_switches.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ios/web/public/features.h"

namespace autofill {
namespace switches {

// The delay between filling two fields.
const char kAutofillIOSDelayBetweenFields[] =
    "autofill-ios-delay-between-fields";

bool IsAutofillIFrameMessagingEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableIFrameSupportOniOS) &&
         base::FeatureList::IsEnabled(web::features::kWebFrameMessaging);
}

}  // namespace switches
}  // namespace autofill
