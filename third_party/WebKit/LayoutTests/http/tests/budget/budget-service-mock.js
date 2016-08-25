/**
 * Mock implementation of the budget service.
 */

"use strict";

const TEST_BUDGET_COST = 1.2;
const TEST_BUDGET_AT = 2.3;
const TEST_BUDGET_TIME = new Date().getTime();

let budgetServiceMock = loadMojoModules(
    'budgetServiceMock',
    ['third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom',
     'mojo/public/js/router'
    ]).then(mojo => {
  const [budgetService, router] = mojo.modules;

  class BudgetServiceMock {
    constructor(interfaceProvider) {
      interfaceProvider.addInterfaceOverrideForTesting(
          budgetService.BudgetService.name,
          handle => this.connectBudgetService_(handle));

      this.interfaceProvider_ = interfaceProvider;
    }

    connectBudgetService_(handle) {
      this.budgetServiceStub_ = new budgetService.BudgetService.stubClass(this);
      this.budgetServiceRouter_ = new router.Router(handle);
      this.budgetServiceRouter_.setIncomingReceiver(this.budgetServiceStub_);
    }

    getCost(operationType) {
      return Promise.resolve({ cost:TEST_BUDGET_COST });
    }

    getBudget() {
      return Promise.resolve({ budget: [ { time:TEST_BUDGET_TIME, budget_at:TEST_BUDGET_AT } ] });
    }
  }
  return new BudgetServiceMock(mojo.interfaces);
});
