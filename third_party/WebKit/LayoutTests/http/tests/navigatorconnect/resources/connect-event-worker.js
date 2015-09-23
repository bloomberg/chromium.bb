importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('../../serviceworker/resources/test-helpers.js');
importScripts('/resources/testharness-helpers.js');
importScripts('test-helpers.js');

// Returns a promise that additionally has |resolve| and |reject| methods.
function NewResolvablePromise() {
  var resolveMethod, rejectMethod;
  var p = new Promise(function(resolve, reject) {
    resolveMethod = resolve;
    rejectMethod = reject;
  });
  p.resolve = resolveMethod;
  p.reject = rejectMethod;
  return p;
}

self.addEventListener('install', function(event) {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', function(event) {
  promise_test(function(test) {
      return wait_for_state(test, self.registration.active, 'activated');
    }, 'wait for worker to be activated');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(event.respondWith({accept: true}));
        });
      return Promise.all([respondResult,
          navigator.services.connect(self.registration.scope + '/service')]);
    }, 'respondWith can synchronously accept a connection without a promise.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              event.respondWith(Promise.resolve({accept: true})));
        });
      return Promise.all([respondResult,
          navigator.services.connect(self.registration.scope + '/service')]);
    }, 'respondWith can synchronously accept a connection with a promise.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              event.respondWith(new Promise(function(resolve) {
                  self.setTimeout(resolve, 1, {accept: true});
                })));
        });
      return Promise.all([respondResult,
          navigator.services.connect(self.registration.scope + '/service')]);
    }, 'respondWith can asynchronously accept a connection.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              promise_rejects(test, "AbortError",
                              event.respondWith({accept: false})));
        });
      return Promise.all([respondResult,
          promise_rejects(test, 'AbortError',
              navigator.services.connect(self.registration.scope + '/service'))
          ]);
    }, 'respondWith can synchronously reject a connection without a promise.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              promise_rejects(test, "AbortError",
                              event.respondWith(Promise.resolve({accept: false}))));
        });
      return Promise.all([respondResult,
          promise_rejects(test, 'AbortError',
              navigator.services.connect(self.registration.scope + '/service'))
          ]);
    }, 'respondWith can synchronously reject a connection with a resolved promise.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              promise_rejects(test, "AbortError",
                              event.respondWith(Promise.reject())));
        });
      return Promise.all([respondResult,
          promise_rejects(test, 'AbortError',
              navigator.services.connect(self.registration.scope + '/service'))
          ]);
    }, 'respondWith can synchronously reject a connection with a rejected promise.');

  promise_test(function(test) {
      navigator.services.onconnect = test.step_func(function(event) {
        });
      return promise_rejects(test, 'AbortError',
          navigator.services.connect(self.registration.scope + '/service'));
    }, 'Not calling respondWith will reject the connection.');

  promise_test(function(test) {
      var respondResult1 = NewResolvablePromise();
      var respondResult2 = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult1.resolve(event.respondWith({accept: true}));
          respondResult2.resolve(
              promise_rejects(test, 'InvalidStateError',
                              event.respondWith({})));
        });
      return Promise.all([respondResult1, respondResult2,
          navigator.services.connect(self.registration.scope + '/service')]);
    }, 'Calling respondWith a second time will fail.');

  promise_test(function(test) {
      var respondResult = NewResolvablePromise();
      navigator.services.onconnect = test.step_func(function(event) {
          respondResult.resolve(
              promise_rejects(test, new TypeError(),
                              event.respondWith("")));
        });
      return Promise.all([respondResult,
          promise_rejects(test, 'AbortError',
              navigator.services.connect(self.registration.scope + '/service'))
          ]);
    }, 'Calling respondWith with something that is not an object rejects.');
});
