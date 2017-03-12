'use strict';

importScripts('/resources/testharness.js');

test(function() {
  assert_own_property(self, 'BackgroundFetchClickEvent');

  // The `tag` and `state` are required in the BackgroundFetchClickEventInit.
  assert_throws(null, () => new BackgroundFetchClickEvent('BackgroundFetchClickEvent'));
  assert_throws(null, () => new BackgroundFetchClickEvent('BackgroundFetchClickEvent', {}));
  assert_throws(null, () => new BackgroundFetchClickEvent('BackgroundFetchClickEvent', { tag: 'foo' }));
  assert_throws(null, () => new BackgroundFetchClickEvent('BackgroundFetchClickEvent', { tag: 'foo', state: 'foo' }));

  // The `state` must be one of { pending, succeeded, failed }. This should not throw.
  for (let state of ['pending', 'succeeded', 'failed'])
    new BackgroundFetchClickEvent('BackgroundFetchClickEvent', { tag: 'foo', state });

  const event = new BackgroundFetchClickEvent('BackgroundFetchClickEvent', {
    tag: 'my-tag',
    state: 'succeeded'
  });

  assert_equals(event.type, 'BackgroundFetchClickEvent');
  assert_equals(event.cancelable, false);
  assert_equals(event.bubbles, false);
  assert_equals(event.tag, 'my-tag');
  assert_equals(event.state, 'succeeded');

  assert_inherits(event, 'waitUntil');

}, 'Verifies that the BackgroundFetchClickEvent can be constructed.');
