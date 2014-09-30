function scope_test(t, worker_url, scope) {
  var expectedScope, options;
  if (scope) {
    expectedScope = new URL(scope, document.location).toString();
  } else {
    expectedScope = new URL('/', document.location).toString();
  }

  var registration;
  var options = scope ? {scope: scope} : {};
  service_worker_unregister(t, scope)
    .then(function() {
        return navigator.serviceWorker.register(worker_url, options);
      })
    .then(function(r) {
        registration = r;
        return wait_for_update(t, registration);
      })
    .then(function(worker) {
        return new Promise(function(resolve) {
            var messageChannel = new MessageChannel();
            messageChannel.port1.onmessage = resolve;
            worker.postMessage('', [messageChannel.port2]);
          });
      })
    .then(function(e) {
        var message = e.data;
        assert_equals(message.initialScope, expectedScope,
                      'Worker should see the scope on eval.');
        assert_equals(message.currentScope, expectedScope,
                      'Worker scope should not change.');
        return registration.unregister();
      })
    .then(function() {
        t.done();
      })
    .catch(unreached_rejection(t));
}
