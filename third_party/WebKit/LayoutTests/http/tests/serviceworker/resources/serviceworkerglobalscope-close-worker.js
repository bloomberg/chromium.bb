importScripts('interfaces.js');
importScripts('worker-test-harness.js');

test(function() {
  assert_throws({name: 'InvalidAccessError'}, function() {
    self.close();
  });
}, 'ServiceWorkerGlobalScope close operation');
