// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

// An array of signatures that only contains the sentinel value and represents
// an empty array.
const PUPData::UwSSignature dummy_signatures[] = {
    {PUPData::kInvalidUwSId,
     PUPData::FLAGS_NONE,
     nullptr,
     PUPData::kMaxFilesToRemoveSmallUwS,
     kNoDisk,
     kNoRegistry,
     kNoCustomMatcher}};

// While testing we will set up special signatures but we still can't have
// uninitialized pointers to pass the DCHECKS during setup.
const PUPData::UwSSignature* PUPData::kRemovedPUPs = dummy_signatures;
const PUPData::UwSSignature* PUPData::kPUPs = dummy_signatures;
const PUPData::UwSSignature* PUPData::kObservedPUPs = dummy_signatures;

}  // namespace chrome_cleaner
