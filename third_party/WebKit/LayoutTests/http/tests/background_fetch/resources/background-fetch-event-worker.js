'use strict';

importScripts('/resources/testharness.js');

test(function() {
  assert_own_property(self, 'BackgroundFetchEvent');

  // The `tag` is required in the BackgroundFetchEventInit.
  assert_throws(null, () => new BackgroundFetchEvent('BackgroundFetchEvent'));
  assert_throws(null, () => new BackgroundFetchEvent('BackgroundFetchEvent', {}));

  const event = new BackgroundFetchEvent('BackgroundFetchEvent', {
    tag: 'my-tag'
  });

  assert_equals(event.type, 'BackgroundFetchEvent');
  assert_equals(event.cancelable, false);
  assert_equals(event.bubbles, false);
  assert_equals(event.tag, 'my-tag');

  assert_inherits(event, 'waitUntil');

}, 'Verifies that the BackgroundFetchEvent can be constructed.');
