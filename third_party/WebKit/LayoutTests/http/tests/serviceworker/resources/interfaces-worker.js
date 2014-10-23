importScripts('interfaces.js');
importScripts('worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    var EVENT_HANDLER = 'object';

    verifyInterface('ServiceWorkerGlobalScope',
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

    verifyInterface('ServiceWorkerClients',
                    self.clients,
                    {
                        getAll: 'function'
                    });

    verifyInterface('ServiceWorkerClient');
    // FIXME: Get an instance and test it, or ensure property exists on prototype.

    verifyInterface('CacheStorage',
                    self.caches,
                    {
                      match: 'function',
                      get: 'function',
                      has: 'function',
                      create: 'function',
                      delete: 'function',
                      keys: 'function'
                    });
  }, 'Interfaces and attributes in ServiceWorkerGlobalScope');

promise_test(function(t) {
    return create_temporary_cache(t)
      .then(function(cache) {
          verifyInterface('Cache',
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
