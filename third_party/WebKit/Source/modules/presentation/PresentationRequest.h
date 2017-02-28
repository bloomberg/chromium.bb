// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationRequest_h
#define PresentationRequest_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/PresentationPromiseProperty.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Vector.h"

namespace blink {

// Implements the PresentationRequest interface from the Presentation API from
// which websites can start or join presentation connections.
class MODULES_EXPORT PresentationRequest final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<PresentationRequest>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationRequest);
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PresentationRequest() = default;

  static PresentationRequest* create(ExecutionContext*,
                                     const String& url,
                                     ExceptionState&);
  static PresentationRequest* create(ExecutionContext*,
                                     const Vector<String>& urls,
                                     ExceptionState&);

  // EventTarget implementation.
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // ScriptWrappable implementation.
  bool hasPendingActivity() const final;

  ScriptPromise start(ScriptState*);
  ScriptPromise reconnect(ScriptState*, const String& id);
  ScriptPromise getAvailability(ScriptState*);

  const Vector<KURL>& urls() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable);

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget implementation.
  void addedEventListener(const AtomicString& eventType,
                          RegisteredEventListener&) override;

 private:
  PresentationRequest(ExecutionContext*, const Vector<KURL>&);

  void recordOriginTypeAccess(ExecutionContext*) const;

  Member<PresentationAvailabilityProperty> m_availabilityProperty;
  Vector<KURL> m_urls;
};

}  // namespace blink

#endif  // PresentationRequest_h
