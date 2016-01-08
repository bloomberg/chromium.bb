// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorMutableStateProvider_h
#define WebCompositorMutableStateProvider_h

#include "public/platform/WebPassOwnPtr.h"

#include <cstdint>

namespace blink {

class WebCompositorMutableState;

// This class is a window onto compositor-owned state. It vends out wrappers
// around per-element bits of this state.
class WebCompositorMutableStateProvider {
public:
    virtual ~WebCompositorMutableStateProvider() { }

    // The caller is expected to take ownership.
    virtual WebPassOwnPtr<WebCompositorMutableState> getMutableStateFor(uint64_t elementId) = 0;
};

} // namespace blink

#endif // WebCompositorMutableStateProvider_h
