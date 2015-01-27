importScripts('../../resources/test-helpers.js');
importScripts('../../resources/worker-testharness.js');

assert_equals(
  self.registration.scope,
  normalizeURL('scope/registration-attribute'),
  'On worker script evaluation, registration attribute should be set');
assert_equals(
  self.registration.installing,
  null,
  'On worker script evaluation, installing worker should be null');
assert_equals(
  self.registration.waiting,
  null,
  'On worker script evaluation, waiting worker should be null');
assert_equals(
  self.registration.active,
  null,
  'On worker script evaluation, active worker should be null');

self.addEventListener('install', function(e) {
    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On install event, registration attribute should be set');
    // FIXME: Verify that...
    //  - registration.installing is set.
    //  - registration.{waiting, active} are null.
    // (http://crbug.com/437677)
  });

self.addEventListener('activate', function(e) {
    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On activate event, registration attribute should be set');
    // FIXME: Verify that...
    //  - registration.{installing, waiting} are null.
    //  - registration.active is set.
    // (http://crbug.com/437677)
  });

self.addEventListener('fetch', function(e) {
    assert_equals(
      self.registration.scope,
      normalizeURL('scope/registration-attribute'),
      'On fetch event, registration attribute should be set');
    // FIXME: Verify that...
    //  - registration.{installing, waiting} are null.
    //  - registration.active is set.
    // (http://crbug.com/437677)
    e.respondWith(new Response('PASS'));
  });
