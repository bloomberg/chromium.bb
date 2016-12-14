// Allows a document to exercise the Budget API within a service worker by sending commands.

self.addEventListener('message', function(workerEvent) {
  port = workerEvent.data;

  // Listen to incoming commands on the message port.
  port.onmessage = function(event) {
    if (typeof event.data != 'object' || !event.data.command)
      return;
    var options = event.data.options || {};
    switch (event.data.command) {

      case 'getCost':
        navigator.budget.getCost('silent-push').then(function(cost) {
          port.postMessage({
            command: event.data.command,
            success: true,
            cost: cost });
        }).catch(makeErrorHandler(event.data.command));
        break;

      case 'getCostInvalidType':
        navigator.budget.getCost('frobinator').then(function(cost) {
          port.postMessage({
            command: event.data.command,
            success: true,
            cost: cost });
        }).catch(makeErrorHandler(event.data.command));
        break;

      case 'getBudget':
        navigator.budget.getBudget().then(budget => {
          port.postMessage({
            command: event.data.command,
            success: true,
            budgetAt: budget[0].budgetAt,
            time: budget[0].time });
        }).catch(makeErrorHandler(event.data.command));
        break;

      case 'reserve':
        navigator.budget.reserve('silent-push').then(success => {
          port.postMessage({
            command: event.data.command,
            success: success });
        }).catch(makeErrorHandler(event.data.command));
        break;

      case 'checkInterfaces':
        let success = false;
        let message = "";
        try {
          success = 'BudgetService' in self &&
              WorkerNavigator.prototype.hasOwnProperty('budget') &&
              BudgetService.prototype.hasOwnProperty('getCost') &&
              BudgetService.prototype.hasOwnProperty('getBudget') &&
              BudgetService.prototype.hasOwnProperty('reserve') &&
              'BudgetState' in self &&
              BudgetState.prototype.hasOwnProperty('budgetAt') &&
              BudgetState.prototype.hasOwnProperty('time');
        } catch(err) {
          message = err.message;
        }
        port.postMessage({
          command: event.data.command,
          success: success,
          message: message});
        break;

      default:
        port.postMessage({
          command: 'error',
          errorMessage: 'Invalid command: ' + event.data.command });
        break;
    }
  };

  // Notify the controller that the worker is now available.
  port.postMessage('ready');
});

function makeErrorHandler(command) {
  return function(error) {
    var errorMessage = error ? error.message : 'unknown error';
    port.postMessage({
      command: command,
      success: false,
      errorMessage: errorMessage });
  };
}
