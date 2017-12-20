// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollSnapData_h
#define ScrollSnapData_h

#include "cc/input/scroll_snap_data.h"

// This file defines classes and structs used in SnapCoordinator.h

namespace blink {

using SnapAxis = cc::SnapAxis;
using SnapStrictness = cc::SnapStrictness;
using SnapAlignment = cc::SnapAlignment;
using ScrollSnapType = cc::ScrollSnapType;
using ScrollSnapAlign = cc::ScrollSnapAlign;
using SnapAreaData = cc::SnapAreaData;
using SnapContainerData = cc::SnapContainerData;

}  // namespace blink

#endif  // ScrollSnapData_h
