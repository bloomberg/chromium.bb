importScripts('../../serviceworker/resources/worker-testharness.js');

test(() => {
  assert_true('PaymentRequestEvent' in self);
  assert_inherits(PaymentRequestEvent.prototype, 'waitUntil');
  assert_own_property(PaymentRequestEvent.prototype, 'respondWith');
  assert_own_property(PaymentRequestEvent.prototype, 'appRequest');
});
