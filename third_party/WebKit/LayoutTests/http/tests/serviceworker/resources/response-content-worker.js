importScripts('worker-test-harness.js');

promise_test(function() {
    var response = new Response('test string');
    assert_equals(
      response.headers.get('Content-Type'),
      'text/plain;charset=UTF-8',
      'A Response constructed with a string should have a Content-Type.');
    return response.body.asText()
      .then(function(text) {
          assert_equals(text, 'test string',
            'Response body text should match the string on construction.');
        });
  }, 'Behavior of Response with string content.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);
    var buffer = intView.buffer;

    var response = new Response(buffer);
    assert_false(response.headers.has('Content-Type'),
      'A Response constructed with ArrayBuffer should not have a content type.');
    return response.body.asArrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [0, 1, 2, 3, 4, 55, 6, 7, 8, 9],
            'Response body ArrayBuffer should match ArrayBuffer ' +
            'it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBuffer content.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);

    var response = new Response(intView);
    assert_false(response.headers.has('Content-Type'),
      'A Response constructed with ArrayBufferView ' +
      'should not have a content type.');
    return response.body.asArrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [0, 1, 2, 3, 4, 55, 6, 7, 8, 9],
            'Response body ArrayBuffer should match ArrayBufferView ' +
            'it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBufferView content without a slice.');

promise_test(function() {
    var intView = new Int32Array([0, 1, 2, 3, 4, 55, 6, 7, 8, 9]);
    var slice = intView.subarray(1, 4);  // Should be [1, 2, 3]
    var response = new Response(slice);
    assert_false(response.headers.has('Content-Type'),
      'A Response constructed with ArrayBufferView ' +
      'should not have a content type.');
    return response.body.asArrayBuffer()
      .then(function(buffer) {
          var resultIntView = new Int32Array(buffer);
          assert_array_equals(
            resultIntView, [1, 2, 3],
            'Response body ArrayBuffer should match ArrayBufferView ' +
            'slice it was constructed with.');
        });
  }, 'Behavior of Response with ArrayBufferView content with a slice.');
