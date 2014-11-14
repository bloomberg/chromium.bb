importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('PushEvent' in self);

    var event = new PushEvent('PushEvent');
    assert_equals(event.type, 'PushEvent');
    assert_own_property(event, 'data');
    assert_inherits(event, 'waitUntil');

}, 'PushEvent is exposed and extends ExtendableEvent.');
