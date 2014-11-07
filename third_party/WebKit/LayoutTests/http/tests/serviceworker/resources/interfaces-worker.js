importScripts('interfaces.js');
importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

var EVENT_HANDLER = 'object';

test(function() {
    verify_interface('ServiceWorkerGlobalScope',
                     self,
                     {
                       scope: 'string',
                       clients: 'object',
                       close: 'function',

                       onactivate: EVENT_HANDLER,
                       onfetch: EVENT_HANDLER,
                       oninstall: EVENT_HANDLER,
                       onmessage: EVENT_HANDLER
                     });
  }, 'ServiceWorkerGlobalScope');

test(function() {
    verify_interface('ServiceWorkerClients',
                     self.clients,
                     {
                       getAll: 'function'
                     });
  }, 'ServiceWorkerClients');

test(function() {
    verify_interface('ServiceWorkerClient');
    // FIXME: Get an instance and test it, or ensure property exists on
    // prototype.
  }, 'ServiceWorkerClient');

test(function() {
    verify_interface('CacheStorage',
                     self.caches,
                     {
                       match: 'function',
                       get: 'function',
                       has: 'function',
                       create: 'function',
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
