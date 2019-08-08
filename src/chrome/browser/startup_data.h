// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_DATA_H_
#define CHROME_BROWSER_STARTUP_DATA_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"

class ChromeFeatureListCreator;

// The StartupData owns any pre-created objects in //chrome before the full
// browser starts, including the ChromeFeatureListCreator and the Profile's
// PrefService (TODO).
class StartupData {
 public:
  StartupData();
  ~StartupData();

  ChromeFeatureListCreator* chrome_feature_list_creator() {
    return chrome_feature_list_creator_.get();
  }

 private:
  std::unique_ptr<ChromeFeatureListCreator> chrome_feature_list_creator_;

  DISALLOW_COPY_AND_ASSIGN(StartupData);
};

#endif  // CHROME_BROWSER_STARTUP_DATA_H_
