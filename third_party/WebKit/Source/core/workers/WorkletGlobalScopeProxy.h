// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScopeProxy_h
#define WorkletGlobalScopeProxy_h

#include "core/CoreExport.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class ScriptSourceCode;
class WorkletPendingTasks;

// A proxy to talk to the worklet global scope. The global scope may exist in
// the main thread or on a different thread.
class CORE_EXPORT WorkletGlobalScopeProxy {
 public:
  virtual ~WorkletGlobalScopeProxy() {}

  // Runs the "fetch and invoke a worklet script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  virtual void FetchAndInvokeScript(const KURL& module_url_record,
                                    WorkletPendingTasks*) {}

  // Evaluates the given script source code. This should be called only for
  // threaded worklets that still use classic script loading.
  virtual void EvaluateScript(const ScriptSourceCode&) = 0;

  // Terminates the worklet global scope from the main thread.
  virtual void TerminateWorkletGlobalScope() = 0;
};

}  // namespace blink

#endif  // WorkletGlobalScopeProxy_h
