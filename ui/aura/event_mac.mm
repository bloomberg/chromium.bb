// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/NSEvent.h>

#include "ui/aura/event_mac.h"

namespace aura {

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
  return [NSEvent eventWithCGEvent:[event CGEvent]];
}

}  // namespace aura
