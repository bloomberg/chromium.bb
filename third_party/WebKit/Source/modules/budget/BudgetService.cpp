// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/BudgetService.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/budget/BudgetState.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/budget_service/budget_service.mojom-blink.h"

namespace blink {

BudgetService::BudgetService()
{
    Platform::current()->interfaceProvider()->getInterface(mojo::GetProxy(&m_service));

    // Set a connection error handler, so that if an embedder doesn't
    // implement a BudgetSerice mojo service, the developer will get a
    // actionable information.
    m_service.set_connection_error_handler(convertToBaseCallback(WTF::bind(&BudgetService::onConnectionError, wrapWeakPersistent(this))));
}

BudgetService::~BudgetService()
{
}

ScriptPromise BudgetService::getCost(ScriptState* scriptState, const AtomicString& /* actionType */)
{
    DCHECK(m_service);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // TODO(harkness): Map the actionType to BudgetOperationType.
    // Get the cost for the action from the browser BudgetService.
    mojom::blink::BudgetOperationType type = mojom::blink::BudgetOperationType::SILENT_PUSH;
    m_service->GetCost(type, convertToBaseCallback(WTF::bind(&BudgetService::gotCost, wrapPersistent(this), wrapPersistent(resolver))));
    return promise;
}

void BudgetService::gotCost(ScriptPromiseResolver* resolver, double cost) const
{
    resolver->resolve(cost);
}

ScriptPromise BudgetService::getBudget(ScriptState* scriptState)
{
    DCHECK(m_service);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    ExecutionContext* context = scriptState->getExecutionContext();
    if (!context)
        return promise;

    // Get the budget from the browser BudgetService.
    RefPtr<SecurityOrigin> origin(context->getSecurityOrigin());
    m_service->GetBudget(origin, convertToBaseCallback(WTF::bind(&BudgetService::gotBudget, wrapPersistent(this), wrapPersistent(resolver))));
    return promise;
}

void BudgetService::gotBudget(ScriptPromiseResolver* resolver, const mojo::WTFArray<mojom::blink::BudgetStatePtr> expectations) const
{
    // Copy the chunks into the budget array.
    HeapVector<Member<BudgetState>> budget(expectations.size());
    for (size_t i = 0; i < expectations.size(); i++)
        budget[i] = new BudgetState(expectations[i]->budget_at, expectations[i]->time);

    resolver->resolve(budget);
}

void BudgetService::onConnectionError()
{
    LOG(ERROR) << "Unable to connect to the Mojo BudgetService.";
    // TODO(harkness): Reject in flight promises.
}

} // namespace blink
