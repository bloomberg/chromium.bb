importScripts('../../serviceworker/resources/worker-testharness.js');

test(() => {
  assert_true('PaymentRequestEvent' in self);
  assert_inherits(PaymentRequestEvent.prototype, 'waitUntil');
  assert_own_property(PaymentRequestEvent.prototype, 'topLevelOrigin');
  assert_own_property(PaymentRequestEvent.prototype, 'paymentRequestOrigin');
  assert_own_property(PaymentRequestEvent.prototype, 'paymentRequestId');
  assert_own_property(PaymentRequestEvent.prototype, 'methodData');
  assert_own_property(PaymentRequestEvent.prototype, 'total');
  assert_own_property(PaymentRequestEvent.prototype, 'modifiers');
  assert_own_property(PaymentRequestEvent.prototype, 'instrumentKey');
  assert_own_property(PaymentRequestEvent.prototype, 'respondWith');
});
