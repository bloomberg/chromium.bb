// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutableStateProvider_h
#define CompositorMutableStateProvider_h

#include "platform/PlatformExport.h"
#include "wtf/PassOwnPtr.h"

#include <cstdint>

namespace cc {
class LayerTreeImpl;
} // namespace cc

namespace blink {

class CompositorMutableState;
struct CompositorMutations;

// This class is a window onto compositor-owned state. It vends out wrappers
// around per-element bits of this state.
class PLATFORM_EXPORT CompositorMutableStateProvider {
public:
    CompositorMutableStateProvider(cc::LayerTreeImpl*, CompositorMutations*);
    ~CompositorMutableStateProvider();

    PassOwnPtr<CompositorMutableState> getMutableStateFor(uint64_t elementId);
private:
    cc::LayerTreeImpl* m_state;
    CompositorMutations* m_mutations;
};

} // namespace blink

#endif // CompositorMutableStateProvider_h
