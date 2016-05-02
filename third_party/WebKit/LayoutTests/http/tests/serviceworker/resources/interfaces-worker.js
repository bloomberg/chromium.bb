importScripts('interfaces.js');
importScripts('worker-testharness.js');

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
                       claim: 'function',
                       matchAll: 'function'
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
    assert_equals(
        new ExtendableEvent('ExtendableEvent').type,
        'ExtendableEvent', 'Type of ExtendableEvent should be ExtendableEvent');
    var req = new Request('https://www.example.com/', {method: 'POST'});
    assert_equals(
        new FetchEvent('FetchEvent', {request: req}).type,
        'FetchEvent', 'Type of FetchEvent should be FetchEvent');
    assert_equals(
        new FetchEvent('FetchEvent', {request: req}).cancelable,
        false, 'Default FetchEvent.cancelable should be false');
    assert_equals(
        new FetchEvent('FetchEvent', {request: req}).bubbles,
        false, 'Default FetchEvent.bubbles should be false');
    assert_equals(
        new FetchEvent('FetchEvent', {request: req}).clientId,
        null, 'Default FetchEvent.clientId should be null');
    assert_equals(
        new FetchEvent('FetchEvent', {request: req}).isReload,
        false, 'Default FetchEvent.isReload should be false');
    assert_equals(
        new FetchEvent(
            'FetchEvent', {request: req, cancelable: false}).cancelable,
            false, 'FetchEvent.cancelable should be false');
    assert_equals(
        new FetchEvent(
            'FetchEvent',
            {request: req,
             clientId: '006e6aae-cfd4-4331-bea8-fbae364703cf'}).clientId,
            '006e6aae-cfd4-4331-bea8-fbae364703cf',
            'FetchEvent.clientId with option {clientId: string} should be ' +
                'the value of string');
    assert_equals(
        new FetchEvent(
            'FetchEvent',
            {request: req, isReload: true}).isReload,
            true,
            'FetchEvent.isReload with option {isReload: true} should be true');
    assert_equals(
        new FetchEvent(
            'FetchEvent',
            {request: req, isReload: true}).request.url,
            'https://www.example.com/',
            'FetchEvent.request.url should return the value it was ' +
                'initialized to');
  }, 'Event constructors');
