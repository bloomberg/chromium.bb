// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup_data.h"

#include "chrome/browser/metrics/chrome_feature_list_creator.h"

StartupData::StartupData()
    : chrome_feature_list_creator_(
          std::make_unique<ChromeFeatureListCreator>()) {}

StartupData::~StartupData() = default;
