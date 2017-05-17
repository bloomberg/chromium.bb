// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentManager.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "modules/payments/PaymentInstruments.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
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

DEFINE_TRACE(PaymentManager) {
  visitor->Trace(registration_);
  visitor->Trace(instruments_);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration), instruments_(nullptr) {
  DCHECK(registration);
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&manager_));

  manager_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this))));

  manager_->Init(registration_->scope());
}

void PaymentManager::OnServiceConnectionError() {
  manager_.reset();
}

}  // namespace blink
