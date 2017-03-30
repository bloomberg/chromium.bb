importScripts('/resources/testharness.js');

test(function() {
    assert_idl_attribute(registration, 'paymentManager', 'One-shot PaymentManager needs to be exposed on the registration.');

    assert_inherits(registration.paymentManager, 'setManifest');
    assert_inherits(registration.paymentManager, 'getManifest');
}, 'PaymentManager should be exposed and have the expected interface.');
