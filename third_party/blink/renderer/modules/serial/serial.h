// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_

#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;
class ScriptState;
class SerialPort;
class SerialPortRequestOptions;

class Serial final : public EventTargetWithInlineData,
                     public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Serial);

 public:
  static Serial* Create(ExecutionContext& executionContext);

  explicit Serial(ExecutionContext&);

  // EventTarget
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)
  ScriptPromise getPorts(ScriptState*);
  ScriptPromise requestPort(ScriptState*, const SerialPortRequestOptions*);

  void Trace(Visitor*) override;

 private:
  void EnsureServiceConnection();
  void OnServiceConnectionError();
  SerialPort* GetOrCreatePort(mojom::blink::SerialPortInfoPtr);
  void OnGetPorts(ScriptPromiseResolver*,
                  Vector<mojom::blink::SerialPortInfoPtr>);
  void OnRequestPort(ScriptPromiseResolver*, mojom::blink::SerialPortInfoPtr);

  mojom::blink::SerialServicePtr service_;
  HeapHashSet<Member<ScriptPromiseResolver>> get_ports_promises_;
  HeapHashSet<Member<ScriptPromiseResolver>> request_port_promises_;
  HeapHashMap<String, WeakMember<SerialPort>> port_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_H_
