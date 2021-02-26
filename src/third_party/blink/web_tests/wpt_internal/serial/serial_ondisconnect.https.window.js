// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  const eventWatcher =
      new EventWatcher(t, navigator.serial, ['connect', 'disconnect']);

  // Wait for getPorts() to resolve in order to ensure that the Mojo client
  // interface has been configured.
  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);

  // Add ports one at a time so that we can map tokens to ports.
  const token1 = fake.addPort();
  const port1 = (await eventWatcher.wait_for(['connect'])).port;

  const token2 = fake.addPort();
  const port2 = (await eventWatcher.wait_for(['connect'])).port;

  fake.removePort(token2);
  const event1 = await eventWatcher.wait_for(['disconnect']);
  assert_true(event1 instanceof SerialConnectionEvent);
  assert_equals(event1.port, port2);

  ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 1);
  assert_equals(ports[0], port1);

  fake.removePort(token1);
  const event2 = await eventWatcher.wait_for(['disconnect']);
  assert_true(event2 instanceof SerialConnectionEvent);
  assert_equals(event2.port, port1);

  ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);
}, 'A "disconnect" event is fired when ports are added.');
