importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('NotificationEvent' in self);

    var event = new NotificationEvent('NotificationEvent');
    assert_equals(event.type, 'NotificationEvent');
    assert_will_be_idl_attribute(event, 'notification');
    assert_will_be_idl_attribute(event, 'action');
    assert_equals(event.cancelable, false);
    assert_equals(event.bubbles, false);
    assert_equals(event.notification, null);
    assert_equals(event.action, "");
    assert_inherits(event, 'waitUntil');

    var eventWithInit = new NotificationEvent('NotificationEvent',
                                              { cancelable: true,
                                                bubbles: true
                                              });
    assert_equals(eventWithInit.cancelable, true);
    assert_equals(eventWithInit.bubbles, true);

}, 'NotificationEvent is exposed, and has the expected interface.');

test(function() {
    assert_will_be_idl_attribute(self, 'onnotificationclick',
                                 'The notificationclick event exists.');

}, 'The notificationclick event exists on the global scope.');
