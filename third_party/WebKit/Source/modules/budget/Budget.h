// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Budget_h
#define Budget_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"

namespace blink {

class ScriptPromise;
class ScriptState;

// This is the entry point into the browser for the Budget API, which is
// designed to give origins the ability to perform background operations
// on the user's behalf.
class Budget final : public GarbageCollected<Budget>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static Budget* create()
    {
        return new Budget();
    }

    ScriptPromise getCost(ScriptState*, const AtomicString& actionType);
    ScriptPromise getBudget(ScriptState*);

    DEFINE_INLINE_TRACE() {}

private:
    Budget();
};

} // namespace blink

#endif // Budget_h
