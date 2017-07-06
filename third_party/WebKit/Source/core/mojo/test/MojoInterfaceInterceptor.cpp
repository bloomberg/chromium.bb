// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/test/MojoInterfaceInterceptor.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/mojo/MojoHandle.h"
#include "core/mojo/test/MojoInterfaceRequestEvent.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

// static
MojoInterfaceInterceptor* MojoInterfaceInterceptor::Create(
    ScriptState* script_state,
    const String& interface_name) {
  return new MojoInterfaceInterceptor(script_state, interface_name);
}

MojoInterfaceInterceptor::~MojoInterfaceInterceptor() {}

void MojoInterfaceInterceptor::start(ExceptionState& exception_state) {
  if (started_)
    return;

  service_manager::InterfaceProvider* interface_provider =
      GetInterfaceProvider();
  if (!interface_provider) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The interface provider is unavailable.");
    return;
  }

  std::string interface_name =
      StringUTF8Adaptor(interface_name_).AsStringPiece().as_string();

  service_manager::InterfaceProvider::TestApi test_api(interface_provider);
  if (test_api.HasBinderForName(interface_name)) {
    exception_state.ThrowDOMException(
        kInvalidModificationError,
        "Interface " + interface_name_ +
            " is already intercepted by another MojoInterfaceInterceptor.");
    return;
  }

  started_ = true;
  test_api.SetBinderForName(interface_name,
                            ConvertToBaseCallback(WTF::Bind(
                                &MojoInterfaceInterceptor::OnInterfaceRequest,
                                WrapWeakPersistent(this))));
}

void MojoInterfaceInterceptor::stop() {
  if (!started_)
    return;

  started_ = false;
  // GetInterfaceProvider() is guaranteed not to return nullptr because this
  // method is called when the context is destroyed.
  service_manager::InterfaceProvider::TestApi test_api(GetInterfaceProvider());
  std::string interface_name =
      StringUTF8Adaptor(interface_name_).AsStringPiece().as_string();
  DCHECK(test_api.HasBinderForName(interface_name));
  test_api.ClearBinderForName(interface_name);
}

DEFINE_TRACE(MojoInterfaceInterceptor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const AtomicString& MojoInterfaceInterceptor::InterfaceName() const {
  return EventTargetNames::MojoInterfaceInterceptor;
}

ExecutionContext* MojoInterfaceInterceptor::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool MojoInterfaceInterceptor::HasPendingActivity() const {
  return started_;
}

void MojoInterfaceInterceptor::ContextDestroyed(ExecutionContext*) {
  stop();
}

MojoInterfaceInterceptor::MojoInterfaceInterceptor(ScriptState* script_state,
                                                   const String& interface_name)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      interface_name_(interface_name) {}

service_manager::InterfaceProvider*
MojoInterfaceInterceptor::GetInterfaceProvider() const {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return nullptr;

  if (context->IsWorkerGlobalScope())
    return &ToWorkerGlobalScope(context)->GetThread()->GetInterfaceProvider();

  LocalFrame* frame = ToDocument(context)->GetFrame();
  if (!frame)
    return nullptr;

  return frame->Client()->GetInterfaceProvider();
}

void MojoInterfaceInterceptor::OnInterfaceRequest(
    mojo::ScopedMessagePipeHandle handle) {
  DispatchEvent(MojoInterfaceRequestEvent::Create(
      MojoHandle::Create(mojo::ScopedHandle::From(std::move(handle)))));
}

}  // namespace blink
