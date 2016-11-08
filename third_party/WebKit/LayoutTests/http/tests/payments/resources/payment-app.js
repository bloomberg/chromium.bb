importScripts('/resources/testharness.js');

test(function() {
    assert_idl_attribute(registration, 'paymentAppManager', 'One-shot PaymentAppManager needs to be exposed on the registration.');

    assert_inherits(registration.paymentAppManager, 'setManifest');
    assert_inherits(registration.paymentAppManager, 'getManifest');
}, 'PaymentAppManager should be exposed and have the expected interface.');
