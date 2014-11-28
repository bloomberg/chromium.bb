importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('/resources/testharness-helpers.js');

test(function() {
    assert_true('PushMessageData' in self);

}, 'PushMessageData is exposed on the Service Worker global scope.');

test(function() {
    var data = new PushMessageData('Hello, world!');
    assert_equals(data.text(), 'Hello, world!');

}, 'PushMessageData can be constructed with a string parameter.');

test(function() {
    var event = new PushEvent('PushEvent');

    assert_equals(event.data.arrayBuffer().byteLength, 0);

    assert_equals(event.data.blob().size, 0);
    assert_equals(event.data.blob().type, '');

    // PushMessageData.json() is specified to be identical to JSON.parse() with
    // the message's data as the argument. JSON.parse('') throws an exception,
    // so verify that calling json() without a body throws the same exception.
    try {
      event.data.json();
    } catch (eventDataException) {
      try {
        JSON.parse('');
      } catch (jsonParseException) {
        assert_equals(eventDataException.name, jsonParseException.name);
        assert_equals(eventDataException.message, jsonParseException.message);
      }
    }

    assert_equals(event.data.text().length, 0);

}, 'PushMessageData is empty by default, given a PushEvent constructor.');

test(function() {
    var event = new PushEvent('PushEvent', {
        data: new PushMessageData('FOOBAR')
    });

    var arrayBuffer = event.data.arrayBuffer();
    assert_equals(arrayBuffer.byteLength, 6);

    var int8Array = new Int8Array(arrayBuffer);
    assert_equals(int8Array[0], 70); // F
    assert_equals(int8Array[3], 66); // B

}, 'Reading an ArrayBuffer from PushMessageData through the PushEvent constructor.');

async_test(function(test) {
    var event = new PushEvent('PushEvent', {
        data: new PushMessageData('FOOBAR')
    });

    var blob = event.data.blob();
    assert_equals(blob.size, 6);
    assert_equals(blob.type, '');

    var reader = new FileReader();
    reader.addEventListener('load', function() {
      assert_equals(reader.result, 'FOOBAR');
      test.done();
    });

    reader.readAsText(blob);

}, 'Reading a Blob from PushMessageData through the PushEvent constructor.')

test(function() {
    var event = new PushEvent('PushEvent', {
        data: new PushMessageData('[5, 6, 7]')
    });

    var array = event.data.json();
    assert_equals(array.length, 3);
    assert_equals(array[1], 6);

    event = new PushEvent('PushEvent', {
        data: new PushMessageData('{ "foo": { "bar": 42 } }')
    });

    assert_equals(event.data.json().foo.bar, 42);

}, 'Reading JSON from PushMessageData through the PushEvent constructor.');

test(function() {
    var event = new PushEvent('PushEvent', {
        data: new PushMessageData('Hello, world!')
    });

    assert_equals(event.data.text(), 'Hello, world!');

}, 'Reading text from PushMessageData through the PushEvent constructor.');
