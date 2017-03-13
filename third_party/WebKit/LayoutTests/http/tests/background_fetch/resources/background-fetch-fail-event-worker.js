'use strict';

importScripts('/resources/testharness.js');

test(function() {
  assert_own_property(self, 'BackgroundFetchFailEvent');

  // The `tag` and `failedFetches` are required options in the
  // BackgroundFetchFailEventInit. The latter must be a sequence of
  // BackgroundFetchSettledRequest instances.
  assert_throws(null, () => new BackgroundFetchFailEvent('BackgroundFetchFailEvent'));
  assert_throws(null, () => new BackgroundFetchFailEvent('BackgroundFetchFailEvent', {}));
  assert_throws(null, () => new BackgroundFetchFailEvent('BackgroundFetchFailEvent', { tag: 'foo' }));
  assert_throws(null, () => new BackgroundFetchFailEvent('BackgroundFetchFailEvent', { tag: 'foo', failedFetches: 'bar' }));

  const failedFetches = [
    new BackgroundFetchSettledRequest(new Request('non-existing-image.png'), new Response()),
    new BackgroundFetchSettledRequest(new Request('non-existing-image-2.png'), new Response())
  ];

  const event = new BackgroundFetchFailEvent('BackgroundFetchFailEvent', {
    tag: 'my-tag',
    failedFetches
  });

  assert_equals(event.type, 'BackgroundFetchFailEvent');
  assert_equals(event.cancelable, false);
  assert_equals(event.bubbles, false);
  assert_equals(event.tag, 'my-tag');

  assert_true(Array.isArray(event.failedFetches));
  assert_array_equals(event.failedFetches, failedFetches);

  assert_inherits(event, 'waitUntil');

}, 'Verifies that the BackgroundFetchFailEvent can be constructed.');
