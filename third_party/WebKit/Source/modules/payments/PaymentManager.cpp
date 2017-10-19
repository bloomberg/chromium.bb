// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "modules/payments/PaymentInstruments.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"

namespace blink {

PaymentManager* PaymentManager::Create(
    ServiceWorkerRegistration* registration) {
  return new PaymentManager(registration);
}

PaymentInstruments* PaymentManager::instruments() {
  if (!instruments_)
    instruments_ = new PaymentInstruments(manager_);
  return instruments_;
}

const String& PaymentManager::userHint() {
  return user_hint_;
}

void PaymentManager::setUserHint(const String& user_hint) {
  user_hint_ = user_hint;
  manager_->SetUserHint(user_hint_);
}

void PaymentManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(instruments_);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration), instruments_(nullptr) {
  DCHECK(registration);

  auto request = mojo::MakeRequest(&manager_);
  ExecutionContext* context = registration->GetExecutionContext();
  if (context && context->IsDocument()) {
    LocalFrame* frame = ToDocument(context)->GetFrame();
    if (frame)
      frame->GetInterfaceProvider().GetInterface(std::move(request));
  } else if (context && context->IsWorkerGlobalScope()) {
    WorkerThread* thread = ToWorkerGlobalScope(context)->GetThread();
    thread->GetInterfaceProvider().GetInterface(std::move(request));
  }

  manager_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this))));
  manager_->Init(registration_->GetExecutionContext()->Url().GetString(),
                 registration_->scope());
}

void PaymentManager::OnServiceConnectionError() {
  manager_.reset();
}

}  // namespace blink
