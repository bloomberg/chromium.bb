// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerGlobalScope_h
#define CompositorWorkerGlobalScope_h

#include "core/dom/FrameRequestCallbackCollection.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/ModulesExport.h"
#include <memory>

namespace blink {

class CompositorWorkerThread;
class InProcessWorkerObjectProxy;
struct GlobalScopeCreationParams;

class MODULES_EXPORT CompositorWorkerGlobalScope final
    : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CompositorWorkerGlobalScope* Create(
      CompositorWorkerThread*,
      std::unique_ptr<GlobalScopeCreationParams>,
      double time_origin);
  ~CompositorWorkerGlobalScope() override;

  void Dispose() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;

  void postMessage(ScriptState*,
                   PassRefPtr<SerializedScriptValue>,
                   const MessagePortArray&,
                   ExceptionState&);
  static bool CanTransferArrayBuffersAndImageBitmaps() { return true; }
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

  int requestAnimationFrame(FrameRequestCallback*);
  void cancelAnimationFrame(int id);
  bool ExecuteAnimationFrameCallbacks(double high_res_time_ms);

  // ExecutionContext:
  bool IsCompositorWorkerGlobalScope() const override { return true; }

  DECLARE_VIRTUAL_TRACE();

 private:
  CompositorWorkerGlobalScope(const KURL&,
                              const String& user_agent,
                              CompositorWorkerThread*,
                              double time_origin,
                              std::unique_ptr<SecurityOrigin::PrivilegeData>,
                              WorkerClients*);

  InProcessWorkerObjectProxy& WorkerObjectProxy() const;

  bool executing_animation_frame_callbacks_;
  FrameRequestCallbackCollection callback_collection_;
};

}  // namespace blink

#endif  // CompositorWorkerGlobalScope_h
