importScripts('/resources/testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_own_property(self, 'SyncManager', 'SyncManager needs to be exposed as a global.');
    assert_idl_attribute(registration, 'sync', 'One-shot SyncManager needs to be exposed on the registration.');

    assert_inherits(registration.sync, 'register');
    assert_inherits(registration.sync, 'getRegistration');
    assert_inherits(registration.sync, 'getRegistrations');
    assert_inherits(registration.sync, 'permissionState');

}, 'SyncManager should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PeriodicSyncManager', 'PeriodicSyncManager needs to be exposed as a global.');
    assert_idl_attribute(registration, 'periodicSync', 'Periodic SyncManager needs to be exposed on the registration.');

    assert_inherits(registration.periodicSync, 'register');
    assert_inherits(registration.periodicSync, 'getRegistration');
    assert_inherits(registration.periodicSync, 'getRegistrations');
    assert_inherits(registration.periodicSync, 'permissionState');

}, 'PeriodicSyncManager should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'SyncRegistration', 'SyncRegistration needs to be exposed as a global.');

    // FIXME: Assert existence of the attributes when they are properly
    // exposed in the prototype chain. https://crbug.com/43394
    assert_own_property(SyncRegistration.prototype, 'finished');
    assert_own_property(SyncRegistration.prototype, 'unregister');
    assert_own_property(SyncRegistration.prototype, 'tag');

}, 'SyncRegistration should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PeriodicSyncRegistration', 'PeriodicSyncRegistration needs to be exposed as a global.');

    // FIXME: Assert existence of the attributes when they are properly
    // exposed in the prototype chain. https://crbug.com/43394

    assert_own_property(PeriodicSyncRegistration.prototype, 'unregister');
    assert_own_property(PeriodicSyncRegistration.prototype, 'tag');

}, 'PeriodicSyncRegistration should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'SyncEvent');

    assert_will_be_idl_attribute(SyncEvent.prototype, 'registration');

    // SyncEvent should be extending ExtendableEvent.
    assert_inherits(SyncEvent.prototype, 'waitUntil');

}, 'SyncEvent should be exposed and have the expected interface.');

test(function() {
    assert_own_property(self, 'PeriodicSyncEvent');

    assert_will_be_idl_attribute(PeriodicSyncEvent.prototype, 'registration');

    // PeriodicSyncEvent should be extending ExtendableEvent.
    assert_inherits(PeriodicSyncEvent.prototype, 'waitUntil');

}, 'PeriodicSyncEvent should be exposed and have the expected interface.');

