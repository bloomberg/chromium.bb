// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_field_trials.h"

#include "base/path_service.h"
#include "components/metrics/persistent_histograms.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

void WebLayerFieldTrials::SetupFieldTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath metrics_dir;
  if (base::PathService::Get(DIR_USER_DATA, &metrics_dir)) {
    InstantiatePersistentHistograms(metrics_dir);
  }
}

}  // namespace weblayer
