importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('NotificationEvent' in self);

    var event = new NotificationEvent('NotificationEvent');
    assert_equals(event.type, 'NotificationEvent');
    assert_own_property(event, 'notification');
    assert_inherits(event, 'waitUntil');

}, 'NotificationEvent is exposed, and has the expected interface.');

test(function() {
    assert_own_property(self, 'onnotificationclick', 'The notificationclick event exists.');
    assert_own_property(self, 'onnotificationerror', 'The notificationerror event exists.');

}, 'The notificationclick and notificationerror events exist on the global scope.');
