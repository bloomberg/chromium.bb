// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/BudgetService.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/budget/BudgetState.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/budget_service/budget_service.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {
namespace {

mojom::blink::BudgetOperationType StringToOperationType(
    const AtomicString& operation) {
  if (operation == "silent-push")
    return mojom::blink::BudgetOperationType::SILENT_PUSH;

  return mojom::blink::BudgetOperationType::INVALID_OPERATION;
}

DOMException* ErrorTypeToException(mojom::blink::BudgetServiceErrorType error) {
  switch (error) {
    case mojom::blink::BudgetServiceErrorType::NONE:
      return nullptr;
    case mojom::blink::BudgetServiceErrorType::DATABASE_ERROR:
      return DOMException::Create(kDataError,
                                  "Error reading the budget database.");
    case mojom::blink::BudgetServiceErrorType::NOT_SUPPORTED:
      return DOMException::Create(kNotSupportedError,
                                  "Requested opration was not supported");
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

BudgetService::BudgetService(
    service_manager::InterfaceProvider* interface_provider) {
  interface_provider->GetInterface(mojo::MakeRequest(&service_));

  // Set a connection error handler, so that if an embedder doesn't
  // implement a BudgetSerice mojo service, the developer will get a
  // actionable information.
  service_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&BudgetService::OnConnectionError, WrapWeakPersistent(this))));
}

BudgetService::~BudgetService() {}

ScriptPromise BudgetService::getCost(ScriptState* script_state,
                                     const AtomicString& operation) {
  DCHECK(service_);

  String error_message;
  if (!ExecutionContext::From(script_state)->IsSecureContext(error_message))
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));

  mojom::blink::BudgetOperationType type = StringToOperationType(operation);
  DCHECK_NE(type, mojom::blink::BudgetOperationType::INVALID_OPERATION);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Get the cost for the action from the browser BudgetService.
  service_->GetCost(type, ConvertToBaseCallback(WTF::Bind(
                              &BudgetService::GotCost, WrapPersistent(this),
                              WrapPersistent(resolver))));
  return promise;
}

void BudgetService::GotCost(ScriptPromiseResolver* resolver,
                            double cost) const {
  resolver->Resolve(cost);
}

ScriptPromise BudgetService::getBudget(ScriptState* script_state) {
  DCHECK(service_);

  String error_message;
  if (!ExecutionContext::From(script_state)->IsSecureContext(error_message))
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Get the budget from the browser BudgetService.
  scoped_refptr<SecurityOrigin> origin(
      ExecutionContext::From(script_state)->GetSecurityOrigin());
  service_->GetBudget(
      origin, ConvertToBaseCallback(WTF::Bind(&BudgetService::GotBudget,
                                              WrapPersistent(this),
                                              WrapPersistent(resolver))));
  return promise;
}

void BudgetService::GotBudget(
    ScriptPromiseResolver* resolver,
    mojom::blink::BudgetServiceErrorType error,
    const WTF::Vector<mojom::blink::BudgetStatePtr> expectations) const {
  if (error != mojom::blink::BudgetServiceErrorType::NONE) {
    resolver->Reject(ErrorTypeToException(error));
    return;
  }

  // Copy the chunks into the budget array.
  HeapVector<Member<BudgetState>> budget(expectations.size());
  for (size_t i = 0; i < expectations.size(); i++) {
    // Return the largest integer less than the budget, so it's easier for
    // developer to reason about budget. Flooring is also significant from a
    // privacy perspective, as we don't want to share precise data as it could
    // aid fingerprinting. See https://crbug.com/710809.
    budget[i] = new BudgetState(floor(expectations[i]->budget_at),
                                expectations[i]->time);
  }

  resolver->Resolve(budget);
}

ScriptPromise BudgetService::reserve(ScriptState* script_state,
                                     const AtomicString& operation) {
  DCHECK(service_);

  mojom::blink::BudgetOperationType type = StringToOperationType(operation);
  DCHECK_NE(type, mojom::blink::BudgetOperationType::INVALID_OPERATION);

  String error_message;
  if (!ExecutionContext::From(script_state)->IsSecureContext(error_message))
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Call to the BudgetService to place the reservation.
  scoped_refptr<SecurityOrigin> origin(
      ExecutionContext::From(script_state)->GetSecurityOrigin());
  service_->Reserve(origin, type,
                    ConvertToBaseCallback(WTF::Bind(
                        &BudgetService::GotReservation, WrapPersistent(this),
                        WrapPersistent(resolver))));
  return promise;
}

void BudgetService::GotReservation(ScriptPromiseResolver* resolver,
                                   mojom::blink::BudgetServiceErrorType error,
                                   bool success) const {
  if (error != mojom::blink::BudgetServiceErrorType::NONE) {
    resolver->Reject(ErrorTypeToException(error));
    return;
  }

  resolver->Resolve(success);
}

void BudgetService::OnConnectionError() {
  LOG(ERROR) << "Unable to connect to the Mojo BudgetService.";
  // TODO(harkness): Reject in flight promises.
}

}  // namespace blink
