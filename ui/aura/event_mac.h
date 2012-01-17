// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_MAC_H_
#define UI_AURA_EVENT_MAC_H_
#pragma once

#include "base/event_types.h"

namespace aura {

// Returns a deep copy of the native |event|.  On Mac the returned value is
// autoreleased.
base::NativeEvent CopyNativeEvent(const base::NativeEvent& event);

}  // namespace aura

#endif  // UI_AURA_EVENT_MAC_H_
