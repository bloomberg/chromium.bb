importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('NotificationEvent' in self);

}, 'NotificationEvent is exposed.');

test(function() {
    assert_own_property(self, 'onnotificationclick', 'The notificationclick event exists.');
    assert_own_property(self, 'onnotificationerror', 'The notificationerror event exists.');

}, 'The notificationclick and notificationerror events exist on the global scope.');
