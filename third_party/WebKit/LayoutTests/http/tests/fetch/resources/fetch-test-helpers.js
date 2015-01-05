if ('ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  // ServiceWorker case
  importScripts('/serviceworker/resources/worker-testharness.js');
  importScripts('/resources/testharness-helpers.js');
} else if (self.importScripts) {
  // Other workers cases
  importScripts('/resources/testharness.js');
  importScripts('/resources/testharness-helpers.js');
}

// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
  return test.step_func(function(error) {
      var reason = error.message || error.name || error;
      var error_prefix = prefix || 'unexpected rejection';
      assert_unreached(error_prefix + ': ' + reason);
    });
}
