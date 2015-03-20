importScripts('/resources/testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_own_property(self, 'SyncManager', 'SyncManager needs to be exposed as a global.');
    assert_will_be_idl_attribute(registration, 'syncManager', 'syncManager needs to be exposed on the registration.');

    assert_inherits(registration.syncManager, 'register');
    assert_inherits(registration.syncManager, 'getRegistration');
    assert_inherits(registration.syncManager, 'getRegistrations');
    // FIXME: Re-enable this once permissions are wired up
    //assert_inherits(registration.syncManager, 'hasPermission');

}, 'SyncManager should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'SyncRegistration', 'PushRegistration needs to be exposed as a global.');

    // FIXME: Assert existence of the attributes when they are properly
    // exposed in the prototype chain. https://crbug.com/43394

    assert_own_property(SyncRegistration.prototype, 'unregister');

}, 'SyncRegistration should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'SyncEvent');

    var event = new SyncEvent('SyncEvent', {id: 'SyncEvent'});
    assert_will_be_idl_attribute(event, 'registration');
    assert_equals(event.type, 'SyncEvent');

    // SyncEvent should be extending ExtendableEvent.
    assert_inherits(event, 'waitUntil');

}, 'SyncEvent should be exposed and have the expected interface.');

