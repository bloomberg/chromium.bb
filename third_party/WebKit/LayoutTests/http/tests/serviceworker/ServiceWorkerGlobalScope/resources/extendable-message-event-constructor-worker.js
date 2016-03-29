importScripts('../../resources/worker-testharness.js');

function createEvent(initializer) {
  if (initializer === undefined)
    return new ExtendableMessageEvent('type');
  return new ExtendableMessageEvent('type', initializer);
}

self.addEventListener('message', function(event) {
    try {
      // These test cases are mostly copied from
      // fast/events/constructors/message-event-constructor.html.

      var test_object = { wanwan: 123 };
      var channel1 = new MessageChannel();
      var channel2 = new MessageChannel();

      // No initializer is passed.
      assert_false(createEvent().bubbles);
      assert_false(createEvent().cancelable);
      assert_equals(createEvent().data, null);
      assert_equals(createEvent().origin, '');
      assert_equals(createEvent().lastEventId, '');
      assert_equals(createEvent().source, null);
      assert_equals(createEvent().ports, null);

      // bubbles is passed.
      assert_false(createEvent({ bubbles: false }).bubbles);
      assert_true(createEvent({ bubbles: true }).bubbles);

      // cancelable is passed.
      assert_false(createEvent({ cancelable: false }).cancelable);
      assert_true(createEvent({ cancelable: true }).cancelable);

      // data is passed.
      assert_equals(createEvent({ data: test_object }).data, test_object);
      assert_equals(createEvent({ data: undefined }).data, null);
      assert_equals(createEvent({ data: null }).data, null);
      assert_equals(createEvent({ data: false }).data, false);
      assert_equals(createEvent({ data: true }).data, true);
      assert_equals(createEvent({ data: '' }).data, '');
      assert_equals(createEvent({ data: 'chocolate' }).data, 'chocolate');
      assert_equals(createEvent({ data: 12345 }).data, 12345);
      assert_equals(createEvent({ data: 18446744073709551615 }).data,
                                18446744073709552000);
      assert_equals(createEvent({ data: NaN }).data, NaN);
      // Note that valueOf() is not called, when the left hand side is
      // evaluated.
      assert_false(
          createEvent({ data: {
              valueOf: function() { return test_object; } } }).data ==
          test_object);
      assert_equals(createEvent({ get data(){ return 123; } }).data, 123);
      assert_throws({ name: 'Error' }, function() {
          createEvent({ get data() { throw { name: 'Error' }; } }); });

      // origin is passed.
      assert_equals(createEvent({ origin: 'melancholy' }).origin, 'melancholy');
      assert_equals(createEvent({ origin: '' }).origin, '');
      assert_equals(createEvent({ origin: null }).origin, 'null');
      assert_equals(createEvent({ origin: false }).origin, 'false');
      assert_equals(createEvent({ origin: true }).origin, 'true');
      assert_equals(createEvent({ origin: 12345 }).origin, '12345');
      assert_equals(
          createEvent({ origin: 18446744073709551615 }).origin,
          '18446744073709552000');
      assert_equals(createEvent({ origin: NaN }).origin, 'NaN');
      assert_equals(createEvent({ origin: [] }).origin, '');
      assert_equals(createEvent({ origin: [1, 2, 3] }).origin, '1,2,3');
      assert_equals(
          createEvent({ origin: { melancholy: 12345 } }).origin,
          '[object Object]');
      // Note that valueOf() is not called, when the left hand side is
      // evaluated.
      assert_equals(
          createEvent({ origin: {
              valueOf: function() { return 'melancholy'; } } }).origin,
          '[object Object]');
      assert_equals(
          createEvent({ get origin() { return 123; } }).origin, '123');
      assert_throws({ name: 'Error' }, function() {
          createEvent({ get origin() { throw { name: 'Error' }; } }); });

      // lastEventId is passed.
      assert_equals(
          createEvent({ lastEventId: 'melancholy' }).lastEventId, 'melancholy');
      assert_equals(createEvent({ lastEventId: '' }).lastEventId, '');
      assert_equals(createEvent({ lastEventId: null }).lastEventId, 'null');
      assert_equals(createEvent({ lastEventId: false }).lastEventId, 'false');
      assert_equals(createEvent({ lastEventId: true }).lastEventId, 'true');
      assert_equals(createEvent({ lastEventId: 12345 }).lastEventId, '12345');
      assert_equals(
          createEvent({ lastEventId: 18446744073709551615 }).lastEventId,
          '18446744073709552000');
      assert_equals(createEvent({ lastEventId: NaN }).lastEventId, 'NaN');
      assert_equals(createEvent({ lastEventId: [] }).lastEventId, '');
      assert_equals(
          createEvent({ lastEventId: [1, 2, 3] }).lastEventId, '1,2,3');
      assert_equals(
          createEvent({ lastEventId: { melancholy: 12345 } }).lastEventId,
          '[object Object]');
      // Note that valueOf() is not called, when the left hand side is
      // evaluated.
      assert_equals(
          createEvent({ lastEventId: {
              valueOf: function() { return 'melancholy'; } } }).lastEventId,
          '[object Object]');
      assert_equals(
          createEvent({ get lastEventId() { return 123; } }).lastEventId,
          '123');
      assert_throws({ name: 'Error' }, function() {
          createEvent({ get lastEventId() { throw { name: 'Error' }; } }); });

      // source is passed.
      assert_equals(createEvent({ source: event.source }).source, event.source);
      assert_equals(
          createEvent({ source: self.registration.active }).source,
          self.registration.active);
      assert_equals(
          createEvent({ source: channel1.port1 }).source, channel1.port1);
      assert_throws(
          { name: 'TypeError' }, function() { createEvent({ source: this }); },
          'source should be Client or ServiceWorker or MessagePort');

      // port is passed.
      // Valid message ports.
      var ports = [channel1.port1, channel1.port2, channel2.port1];
      var passed_ports = createEvent({ ports: ports}).ports;
      assert_equals(passed_ports[0], channel1.port1);
      assert_equals(passed_ports[1], channel1.port2);
      assert_equals(passed_ports[2], channel2.port1);
      assert_equals(createEvent({ ports: [] }).ports.length, 0);
      assert_equals(createEvent({ ports: undefined }).ports, null);
      assert_equals(createEvent({ ports: null }).ports, null);

      // Invalid message ports.
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: [1, 2, 3] }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: test_object }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: this }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: false }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: true }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: '' }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: 'chocolate' }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: 12345 }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: 18446744073709551615 }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ ports: NaN }); });
      assert_throws({ name: 'TypeError' },
          function() { createEvent({ get ports() { return 123; } }); });
      assert_throws({ name: 'Error' }, function() {
          createEvent({ get ports() { throw { name: 'Error' }; } }); });
      // Note that valueOf() is not called, when the left hand side is
      // evaluated.
      assert_throws({ name: 'TypeError' }, function() {
          createEvent({ ports: { valueOf: function() { return ports; } } }); });

      // All initializers are passed.
      var initializers = {
          bubbles: true,
          cancelable: true,
          data: test_object,
          origin: 'wonderful',
          lastEventId: 'excellent',
          source: event.source,
          ports: ports
      };
      assert_equals(createEvent(initializers).bubbles, true);
      assert_equals(createEvent(initializers).cancelable, true);
      assert_equals(createEvent(initializers).data, test_object);
      assert_equals(createEvent(initializers).origin, 'wonderful');
      assert_equals(createEvent(initializers).lastEventId, 'excellent');
      assert_equals(createEvent(initializers).source, event.source);
      assert_equals(createEvent(initializers).ports[0], ports[0]);
      assert_equals(createEvent(initializers).ports[1], ports[1]);
      assert_equals(createEvent(initializers).ports[2], ports[2]);

      event.source.postMessage('success');
    } catch(e) {
      event.source.postMessage('failure: ' + e.message);
    }
  });
