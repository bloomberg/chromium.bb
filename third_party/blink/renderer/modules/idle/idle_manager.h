// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/idle/idle_options.h"
#include "third_party/blink/renderer/modules/idle/idle_status.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class IdleOptions;
class IdleStatus;
class ExecutionContext;
class ExceptionState;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;

class IdleManager final : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(IdleManager);
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit IdleManager(ExecutionContext*);

  // IdleManager IDL interface.
  ScriptPromise query(ScriptState*,
                      const IdleOptions* options,
                      ExceptionState&);

  static IdleManager* Create(ExecutionContext* context);
  void Trace(blink::Visitor*) override;

 private:
  void OnIdleManagerConnectionError();
  void OnAddMonitor(ScriptPromiseResolver*,
                    IdleStatus*,
                    mojom::blink::IdleState);

  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  mojom::blink::IdleManagerPtr service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_
