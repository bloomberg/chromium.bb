'use strict';

importScripts('/resources/testharness.js');

test(function() {
  assert_own_property(self, 'BackgroundFetchedEvent');

  // The `tag` and `fetches` are required options in the
  // BackgroundFetchedEventInit. The latter must be a sequence of
  // BackgroundFetchSettledFetch instances.
  assert_throws(null, () => new BackgroundFetchedEvent('BackgroundFetchedEvent'));
  assert_throws(null, () => new BackgroundFetchedEvent('BackgroundFetchedEvent', {}));
  assert_throws(null, () => new BackgroundFetchedEvent('BackgroundFetchedEvent', { tag: 'foo' }));
  assert_throws(null, () => new BackgroundFetchedEvent('BackgroundFetchedEvent', { tag: 'foo', fetches: 'bar' }));

  const fetches = [
    new BackgroundFetchSettledFetch(new Request('non-existing-image.png'), new Response()),
    new BackgroundFetchSettledFetch(new Request('non-existing-image-2.png'), new Response())
  ];

  const event = new BackgroundFetchedEvent('BackgroundFetchedEvent', {
    tag: 'my-tag',
    fetches
  });

  assert_equals(event.type, 'BackgroundFetchedEvent');
  assert_equals(event.cancelable, false);
  assert_equals(event.bubbles, false);
  assert_equals(event.tag, 'my-tag');

  assert_true(Array.isArray(event.fetches));
  assert_array_equals(event.fetches, fetches);

  assert_inherits(event, 'updateUI');
  assert_inherits(event, 'waitUntil');

}, 'Verifies that the BackgroundFetchedEvent can be constructed.');
