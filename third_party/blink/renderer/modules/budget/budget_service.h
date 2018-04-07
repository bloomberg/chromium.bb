// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUDGET_BUDGET_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUDGET_BUDGET_SERVICE_H_

#include "third_party/blink/public/platform/modules/budget_service/budget_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

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
class BudgetService final : public ScriptWrappable {
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

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUDGET_BUDGET_SERVICE_H_
