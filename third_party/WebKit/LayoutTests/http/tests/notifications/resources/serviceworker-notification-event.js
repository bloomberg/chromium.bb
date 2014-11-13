importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('NotificationEvent' in self);

}, 'NotificationEvent is exposed.');
