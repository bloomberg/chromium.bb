importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('PushEvent' in self);

    var event = new PushEvent('PushEvent');
    assert_equals(event.type, 'PushEvent');
    assert_own_property(event, 'data');
    assert_equals(event.cancelable, false);
    assert_equals(event.bubbles, false);
    assert_inherits(event, 'waitUntil');

    var data = new PushMessageData('foo');
    var eventWithInit = new PushEvent('PushEvent',
                                      { cancelable: true,
                                        bubbles: true,
                                        data: data,
                                      });
    assert_equals(eventWithInit.cancelable, true);
    assert_equals(eventWithInit.bubbles, true);
    assert_equals(eventWithInit.data, data);

}, 'PushEvent is exposed and extends ExtendableEvent.');
