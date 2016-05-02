importScripts('/resources/testharness.js');

test(function() {
    assert_own_property(self, 'PushEvent');

    var event = new PushEvent('PushEvent');
    assert_idl_attribute(event, 'data');
    assert_equals(event.type, 'PushEvent');

    // PushEvent should be extending ExtendableEvent.
    assert_inherits(event, 'waitUntil');

}, 'PushEvent should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PushManager', 'PushManager needs to be exposed as a global.');
    assert_idl_attribute(registration, 'pushManager', 'PushManager needs to be exposed on the registration.');

    assert_inherits(registration.pushManager, 'subscribe');
    assert_inherits(registration.pushManager, 'getSubscription');
    assert_inherits(registration.pushManager, 'permissionState');

}, 'PushManager should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PushMessageData', 'PushMessageData needs to be exposed as a global.');

    var pushMessageData = new PushEvent('PushEvent', { data: 'SomeData' }).data;
    assert_inherits(pushMessageData, 'arrayBuffer');
    assert_inherits(pushMessageData, 'blob');
    assert_inherits(pushMessageData, 'json');
    assert_inherits(pushMessageData, 'text');

}, 'PushMessageData should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PushSubscription', 'PushSubscription needs to be exposed as a global.');

    assert_own_property(PushSubscription.prototype, 'endpoint');

    assert_own_property(PushSubscription.prototype, 'getKey');
    assert_own_property(PushSubscription.prototype, 'unsubscribe');

}, 'PushSubscription should be exposed and have the expected interface.');
