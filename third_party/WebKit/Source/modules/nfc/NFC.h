// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"

namespace blink {

class NFC final
    : public GarbageCollectedFinalized<NFC>
    , public ScriptWrappable
    , public LocalFrameLifecycleObserver
    , public PageLifecycleObserver {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NFC);

public:
    static NFC* create(LocalFrame*);
#if ENABLE(OILPAN)
    ~NFC();
#else
    ~NFC() override;
#endif

    // Get an adapter object providing NFC functionality.
    ScriptPromise requestAdapter(ScriptState*);

    // Implementation of LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    // Implementation of PageLifecycleObserver
    void pageVisibilityChanged() override;

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

private:
    NFC(LocalFrame*);
};

} // namespace blink

#endif // NFC_h
