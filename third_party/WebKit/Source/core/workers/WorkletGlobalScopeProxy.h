// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScopeProxy_h
#define WorkletGlobalScopeProxy_h

#include "core/CoreExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class WorkletModuleResponsesMap;
class WorkletPendingTasks;

// Abstracts communication from (Main/Threaded)Worklet on the main thread to
// (Main/Threaded)WorkletGlobalScope so that Worklet class doesn't have to care
// about the thread WorkletGlobalScope runs on.
class CORE_EXPORT WorkletGlobalScopeProxy : public GarbageCollectedMixin {
 public:
  virtual ~WorkletGlobalScopeProxy() {}

  // Runs the "fetch and invoke a worklet script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  virtual void FetchAndInvokeScript(
      const KURL& module_url_record,
      WorkletModuleResponsesMap*,
      WebURLRequest::FetchCredentialsMode,
      RefPtr<WebTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*) = 0;

  // Notifies that the Worklet object is destroyed. This should be called in the
  // destructor of the Worklet object. This may call
  // ThreadedMessagingProxy::ParentObjectDestroyed() and cause deletion of
  // |this|. See comments on ParentObjectDestroyed() for details.
  virtual void WorkletObjectDestroyed() = 0;

  // Terminates the worklet global scope from the main thread.
  virtual void TerminateWorkletGlobalScope() = 0;
};

}  // namespace blink

#endif  // WorkletGlobalScopeProxy_h
