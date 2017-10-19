// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BudgetService_h
#define BudgetService_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "public/platform/modules/budget_service/budget_service.mojom-blink.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// This is the entry point into the browser for the BudgetService API, which is
// designed to give origins the ability to perform background operations
// on the user's behalf.
class BudgetService final : public GarbageCollectedFinalized<BudgetService>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BudgetService* Create(
      service_manager::InterfaceProvider* interface_provider) {
    return new BudgetService(interface_provider);
  }

  ~BudgetService();

  // Implementation of the Budget API interface.
  ScriptPromise getCost(ScriptState*, const AtomicString& operation);
  ScriptPromise getBudget(ScriptState*);
  ScriptPromise reserve(ScriptState*, const AtomicString& operation);

  void Trace(blink::Visitor* visitor) {}

 private:
  // Callbacks from the BudgetService to the blink layer.
  void GotCost(ScriptPromiseResolver*, double cost) const;
  void GotBudget(
      ScriptPromiseResolver*,
      mojom::blink::BudgetServiceErrorType,
      const WTF::Vector<mojom::blink::BudgetStatePtr> expectations) const;
  void GotReservation(ScriptPromiseResolver*,
                      mojom::blink::BudgetServiceErrorType,
                      bool success) const;

  // Error handler for use if mojo service doesn't connect.
  void OnConnectionError();

  explicit BudgetService(service_manager::InterfaceProvider*);

  // Pointer to the Mojo service which will proxy calls to the browser.
  mojom::blink::BudgetServicePtr service_;
};

}  // namespace blink

#endif  // BudgetService_h
