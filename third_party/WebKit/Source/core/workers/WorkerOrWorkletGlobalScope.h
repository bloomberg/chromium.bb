// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletGlobalScope_h
#define WorkerOrWorkletGlobalScope_h

#include "core/dom/ExecutionContext.h"

namespace blink {

class ExecutionContext;
class ScriptWrappable;
class WorkerOrWorkletScriptController;

class CORE_EXPORT WorkerOrWorkletGlobalScope : public ExecutionContext {
public:
    virtual ScriptWrappable* scriptWrappable() const = 0;
    virtual WorkerOrWorkletScriptController* script() = 0;
};

DEFINE_TYPE_CASTS(WorkerOrWorkletGlobalScope, ExecutionContext, context, (context->isWorkerGlobalScope() || context->isWorkletGlobalScope()), (context.isWorkerGlobalScope() || context.isWorkletGlobalScope()));

} // namespace blink

#endif // WorkerOrWorkletGlobalScope_h
