// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BudgetService_h
#define BudgetService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"

namespace blink {

class ScriptPromise;
class ScriptState;

// This is the entry point into the browser for the BudgetService API, which is
// designed to give origins the ability to perform background operations
// on the user's behalf.
class BudgetService final : public GarbageCollected<BudgetService>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static BudgetService* create()
    {
        return new BudgetService();
    }

    ScriptPromise getCost(ScriptState*, const AtomicString& actionType);
    ScriptPromise getBudget(ScriptState*);

    DEFINE_INLINE_TRACE() {}

private:
    BudgetService();
};

} // namespace blink

#endif // BudgetService_h
