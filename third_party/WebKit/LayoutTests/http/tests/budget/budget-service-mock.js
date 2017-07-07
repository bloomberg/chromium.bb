/**
 * Mock implementation of the budget service.
 */

"use strict";

const TEST_BUDGET_COST = 1.2;
const TEST_BUDGET_AT = 2;
const TEST_BUDGET_TIME = new Date().getTime();

let budgetServiceMock = loadMojoModules(
    'budgetServiceMock',
    ['third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom',
     'mojo/public/js/bindings'
    ]).then(mojo => {
  const [budgetService, bindings] = mojo.modules;

  class BudgetServiceMock {
    constructor() {
      // Register process-wide (worker) interface override.
      mojo.interfaces.addInterfaceOverrideForTesting(
          budgetService.BudgetService.name,
          handle => this.bindingSet_.addBinding(this, handle));

      // Register frame interface override.
      mojo.frameInterfaces.addInterfaceOverrideForTesting(
          budgetService.BudgetService.name,
          handle => this.bindingSet_.addBinding(this, handle));

      // Values to return for the next getBudget and getCost calls.
      this.cost_ = {};
      this.budget_ = [];
      this.error_ = budgetService.BudgetServiceErrorType.NONE;
      this.bindingSet_ = new bindings.BindingSet(budgetService.BudgetService);
    }

    // This is called directly from test JavaScript to set up the return value
    // for the getCost Mojo call. The operationType mapping is needed to mirror
    // the mapping that happens in the Mojo layer.
    setCost(operationType, cost) {
      let mojoOperationType = budgetService.BudgetOperationType.INVALID_OPERATION;
      if (operationType == "silent-push")
        mojoOperationType = budgetService.BudgetOperationType.SILENT_PUSH;

      this.cost_[mojoOperationType] = cost;
    }


    // This is called directly from test JavaScript to set up the budget that is
    // returned from a later getBudget Mojo call. This adds an entry into the
    // budget array.
    addBudget(addTime, addBudget) {
      this.budget_.push({ time: addTime, budget_at: addBudget });
    }

    // This is called from test JavaScript. It sets whether the next reserve
    // call should return success or not.
    setReserveSuccess(success) {
      this.success_ = success;
    }

    // Called from test JavaScript, this sets the error to be returned by the
    // Mojo service to the BudgetService in Blink. This error is never surfaced
    // to the test JavaScript, but the test code may get an exception if one of
    // these is set.
    setError(errorName) {
      switch (errorName) {
        case "database-error":
          this.error_ = budgetService.BudgetServiceErrorType.DATABASE_ERROR;
          break;
        case "not-supported":
          this.error_ = budgetService.BudgetServiceErrorType.NOT_SUPPORTED;
          break;
        case "no-error":
          this.error_ = budgetService.BudgetServiceErrorType.NONE;
          break;
      }
    }

    // This provides an implementation for the Mojo serivce getCost call.
    getCost(operationType) {
      return Promise.resolve({ cost: this.cost_[operationType] });
    }

    // This provides an implementation for the Mojo serivce getBudget call.
    getBudget() {
      return Promise.resolve({ error_type: this.error_, budget: this.budget_ });
    }

    // This provides an implementation for the Mojo serivce reserve call.
    reserve() {
      return Promise.resolve({ error_type: this.error_, success: this.success_ });
    }
  }
  return new BudgetServiceMock();
});
