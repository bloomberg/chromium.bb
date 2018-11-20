// Create a ReadableStream that will pass the tests in
// testTransferredReadableStream(), below.
function createOriginalReadableStream() {
  return new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.close();
    }
  });
}

// Common tests to roughly determine that |rs| is a correctly transferred
// version of a stream created by createOriginalReadableStream().
function testTransferredReadableStream(rs) {
  assert_equals(rs.constructor, ReadableStream,
                'rs should be a ReadableStream in this realm');
  assert_true(rs instanceof ReadableStream,
              'instanceof check should pass');

  // Perform a brand-check on |rs| in the process of calling getReader().
  const reader = ReadableStream.prototype.getReader.call(rs);

  return reader.read().then(({value, done}) => {
    assert_false(done, 'done should be false');
    assert_equals(value, 'a', 'value should be "a"');
    return reader.read();
  }).then(({value, done}) => {
    assert_true(done, 'done should be true');
  });
}

function testMessage(msg) {
  assert_array_equals(msg.ports, [], 'there should be no ports in the event');
  return testTransferredReadableStream(msg.data);
}

function testMessageEvent(target) {
  return new Promise((resolve, reject) => {
    target.addEventListener('message', ev => {
      try {
        resolve(testMessage(ev));
      } catch(e) {
        reject(e);
      }
    }, {once: true});
  });
}

function testMessageEventOrErrorMessage(target) {
  return new Promise((resolve, reject) => {
    target.addEventListener('message', ev => {
      if (typeof ev.data === 'string') {
        // Assume it's an error message and reject with it.
        reject(ev.data);
        return;
      }

      try {
        resolve(testMessage(ev));
      } catch(e) {
        reject(e);
      }
    }, {once: true});
  });
}

function checkTestResults(target) {
  return new Promise((resolve, reject) => {
    target.onmessage = msg => {
      // testharness.js sends us objects which we need to ignore.
      if (typeof msg.data !== 'string')
        return;

      if (msg.data === 'OK') {
        resolve();
      } else {
        reject(msg.data);
      }
    };
  });
}
