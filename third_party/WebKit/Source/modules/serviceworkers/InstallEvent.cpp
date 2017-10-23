// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/InstallEvent.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit& event_init) {
  return new InstallEvent(type, event_init);
}

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit& event_init,
                                   int event_id,
                                   WaitUntilObserver* observer) {
  return new InstallEvent(type, event_init, event_id, observer);
}

InstallEvent::~InstallEvent() {}

void InstallEvent::registerForeignFetch(ScriptState* script_state,
                                        const ForeignFetchOptions& options,
                                        ExceptionState& exception_state) {
  if (!IsBeingDispatched()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The event handler is already finished.");
    return;
  }

  if (!options.hasOrigins() || options.origins().IsEmpty()) {
    exception_state.ThrowTypeError("At least one origin is required");
    return;
  }
  const Vector<String>& origin_list = options.origins();

  // The origins parameter is either just a "*" to indicate all origins, or an
  // explicit list of origins as absolute URLs. Internally an empty list of
  // origins is used to represent the "*" case though.
  Vector<scoped_refptr<SecurityOrigin>> parsed_origins;
  if (origin_list.size() != 1 || origin_list[0] != "*") {
    parsed_origins.resize(origin_list.size());
    for (size_t i = 0; i < origin_list.size(); ++i) {
      parsed_origins[i] = SecurityOrigin::CreateFromString(origin_list[i]);
      // Invalid URLs will result in a unique origin. And in general
      // unique origins should not be accepted.
      if (parsed_origins[i]->IsUnique()) {
        exception_state.ThrowTypeError("Invalid origin URL: " + origin_list[i]);
        return;
      }
    }
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  ServiceWorkerGlobalScopeClient* client =
      ServiceWorkerGlobalScopeClient::From(execution_context);

  String scope_path = static_cast<KURL>(client->Scope()).GetPath();
  scoped_refptr<SecurityOrigin> origin = execution_context->GetSecurityOrigin();

  if (!options.hasScopes() || options.scopes().IsEmpty()) {
    exception_state.ThrowTypeError("At least one scope is required");
    return;
  }
  const Vector<String>& sub_scopes = options.scopes();
  Vector<KURL> sub_scope_urls(sub_scopes.size());
  for (size_t i = 0; i < sub_scopes.size(); ++i) {
    sub_scope_urls[i] = execution_context->CompleteURL(sub_scopes[i]);
    if (!sub_scope_urls[i].IsValid()) {
      exception_state.ThrowTypeError("Invalid subscope URL: " + sub_scopes[i]);
      return;
    }
    sub_scope_urls[i].RemoveFragmentIdentifier();
    if (!origin->CanRequest(sub_scope_urls[i])) {
      exception_state.ThrowTypeError("Subscope URL is not within scope: " +
                                     sub_scopes[i]);
      return;
    }
    String sub_scope_path = sub_scope_urls[i].GetPath();
    if (!sub_scope_path.StartsWith(scope_path)) {
      exception_state.ThrowTypeError("Subscope URL is not within scope: " +
                                     sub_scopes[i]);
      return;
    }
  }
  client->RegisterForeignFetchScopes(event_id_, sub_scope_urls, parsed_origins);
}

const AtomicString& InstallEvent::InterfaceName() const {
  return EventNames::InstallEvent;
}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit& initializer)
    : ExtendableEvent(type, initializer), event_id_(0) {}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit& initializer,
                           int event_id,
                           WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer), event_id_(event_id) {}

}  // namespace blink
