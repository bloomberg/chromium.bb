// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"

namespace blink {

class NFC final
    : public GarbageCollectedFinalized<NFC>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public DOMWindowProperty {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NFC);

public:
    static NFC* create(ExecutionContext*, LocalFrame*);
    ~NFC() override;

    // Get an adapter object providing NFC functionality.
    ScriptPromise requestAdapter(ScriptState*);

    DECLARE_VIRTUAL_TRACE();

private:
    NFC(ExecutionContext*, LocalFrame*);
};

} // namespace blink

#endif // NFC_h
