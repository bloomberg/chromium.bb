// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_features.h"

namespace enterprise_signals::features {

const base::Feature kNewEvSignalsEnabled = {"NewEvSignalsEnabled",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kDisableFileSystemInfo{
    &kNewEvSignalsEnabled, "DisableFileSystemInfo", false};
const base::FeatureParam<bool> kDisableSettings{&kNewEvSignalsEnabled,
                                                "DisableSettings", false};
const base::FeatureParam<bool> kDisableAntiVirus{&kNewEvSignalsEnabled,
                                                 "DisableAntiVirus", false};
const base::FeatureParam<bool> kDisableHotfix{&kNewEvSignalsEnabled,
                                              "DisableHotfix", false};

bool IsNewFunctionEnabled(NewEvFunction new_ev_function) {
  if (!base::FeatureList::IsEnabled(kNewEvSignalsEnabled)) {
    return false;
  }

  bool disable_function = false;
  switch (new_ev_function) {
    case NewEvFunction::kFileSystemInfo:
      disable_function = kDisableFileSystemInfo.Get();
      break;
    case NewEvFunction::kSettings:
      disable_function = kDisableSettings.Get();
      break;
    case NewEvFunction::kAntiVirus:
      disable_function = kDisableAntiVirus.Get();
      break;
    case NewEvFunction::kHotfix:
      disable_function = kDisableHotfix.Get();
      break;
  }
  return !disable_function;
}

}  // namespace enterprise_signals::features
