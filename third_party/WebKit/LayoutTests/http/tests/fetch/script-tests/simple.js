if ('ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  // ServiceWorker case
  importScripts('/serviceworker/resources/worker-testharness.js');
} else if (self.importScripts) {
  // Other workers cases
  importScripts('/resources/testharness.js');
}

promise_test(function(test) {
    return Promise.resolve().then(function() {
        return fetch(new Request('/fetch/resources/hello.txt'));
      }).then(function(res) {
        return res.text();
      }).then(function(text) {
        assert_equals(text, 'hello, world\n', 'response.body');
      });
  });

done();
