// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoInterfaceInterceptor_h
#define MojoInterfaceInterceptor_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventListener.h"
#include "core/dom/events/EventTarget.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class ExceptionState;
class ExecutionContext;

// A MojoInterfaceInterceptor can be constructed by test scripts in order to
// intercept all outgoing requests for a specific named interface from the
// owning document, whether the requests come from other script or from native
// code (e.g. native API implementation code.) In production, such reqiests are
// normally routed to the browser to be bound to real interface implementations,
// but in test environments it's often useful to mock them out locally.
class MojoInterfaceInterceptor final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MojoInterfaceInterceptor>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MojoInterfaceInterceptor);

 public:
  static MojoInterfaceInterceptor* Create(ExecutionContext*,
                                          const String& interface_name,
                                          const String& scope,
                                          ExceptionState&);
  ~MojoInterfaceInterceptor();

  void start(ExceptionState&);
  void stop();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(interfacerequest);

  virtual void Trace(blink::Visitor*);

  // EventTargetWithInlineData
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

 private:
  MojoInterfaceInterceptor(ExecutionContext*,
                           const String& interface_name,
                           bool process_scope);

  service_manager::InterfaceProvider* GetInterfaceProvider() const;
  void OnInterfaceRequest(mojo::ScopedMessagePipeHandle);
  void DispatchInterfaceRequestEvent(mojo::ScopedMessagePipeHandle);

  const String interface_name_;
  bool started_ = false;
  bool process_scope_ = false;
};

}  // namespace blink

#endif  // MojoInterfaceInterceptor_h
