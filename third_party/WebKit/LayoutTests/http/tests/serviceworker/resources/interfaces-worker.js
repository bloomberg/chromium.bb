importScripts('interfaces.js');
importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

var EVENT_HANDLER = 'object';

test(function() {
    verify_interface('ServiceWorkerGlobalScope',
                     self,
                     {
                       clients: 'object',
                       close: 'function',
                       registration: 'object',
                       skipWaiting: 'function',

                       onactivate: EVENT_HANDLER,
                       onfetch: EVENT_HANDLER,
                       oninstall: EVENT_HANDLER,
                       onmessage: EVENT_HANDLER
                     });
  }, 'ServiceWorkerGlobalScope');

test(function() {
    verify_interface('Clients',
                     self.clients,
                     {
                       getAll: 'function'
                     });
  }, 'Clients');

test(function() {
    verify_interface('Client');
    // FIXME: Get an instance and test it, or ensure property exists on
    // prototype.
  }, 'Client');

test(function() {
    verify_interface('WindowClient');
    // FIXME: Get an instance and test it, or ensure property exists on
    // prototype.
  }, 'WindowClient');

test(function() {
    verify_interface('CacheStorage',
                     self.caches,
                     {
                       match: 'function',
                       has: 'function',
                       open: 'function',
                       delete: 'function',
                       keys: 'function'
                     });
  }, 'CacheStorage');

promise_test(function(t) {
    return create_temporary_cache(t)
      .then(function(cache) {
          verify_interface('Cache',
                           cache,
                           {
                             match: 'function',
                             matchAll: 'function',
                             add: 'function',
                             addAll: 'function',
                             put: 'function',
                             delete: 'function',
                             keys: 'function'
                           });
        });
  }, 'Cache');

test(function() {
    assert_equals(new ExtendableEvent('ExtendableEvent').type, 'ExtendableEvent');
    assert_equals(new InstallEvent('InstallEvent').type, 'InstallEvent');

    assert_equals(new InstallEvent('InstallEvent').cancelable, false);
    assert_equals(new InstallEvent('InstallEvent').bubbles, false);
    assert_equals(new InstallEvent('InstallEvent', { cancelable : true }).cancelable, true);
    assert_equals(new InstallEvent('InstallEvent', { bubbles : true }).bubbles, true);
  }, 'Event constructors');
