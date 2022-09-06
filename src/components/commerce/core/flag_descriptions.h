// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
#define COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_

namespace commerce::flag_descriptions {

// Enables the user to track prices of the Shopping URLs they are visiting.
// The first variation is to display price drops in the Tab Switching UI when
// they are identified.
extern const char kCommercePriceTrackingName[];
extern const char kCommercePriceTrackingDescription[];

}  // namespace commerce::flag_descriptions

#endif  // COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
