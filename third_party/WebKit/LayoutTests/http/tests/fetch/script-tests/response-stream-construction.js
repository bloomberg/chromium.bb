// This file contains tests for Response construction with a readable stream.
// Move these tests to response.js once the feature gets stable.

if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
  importScripts('/streams/resources/rs-utils.js');
}

test(() => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});

    var response = new Response(stream);
    // TODO(yhirano): This should be assert_equals.
    assert_not_equals(response.body, stream);
  }, 'Response constructed with a stream');

promise_test(() => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    controller.enqueue(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]));
    controller.enqueue(new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64]));
    controller.close();
    assert_false(stream.locked);
    var response = new Response(stream);
    var p = response.text().then(t => {
        assert_equals(t, 'helloworld');
      });
    assert_true(stream.locked);
    return p;
  }, 'Response constructed with a stream');

promise_test(() => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    controller.enqueue(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]));
    controller.enqueue(new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64]));
    controller.close();

    var response = new Response(stream);
    return readableStreamToArray(response.body).then(chunks => {
        var decoder = new TextDecoder('utf-8');
        var r = '';
        for (var chunk of chunks) {
          r += decoder.decode(chunk, {stream: true});
        }
        r += decoder.decode();
        assert_equals(r, 'helloworld');
      });
  }, 'Response constructed with a stream / Read from body stream');

promise_test(t => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    setTimeout(() => {
        controller.enqueue(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]));
        controller.enqueue(new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64]));
        controller.error();
    }, 1);
    var response = new Response(stream);
    return promise_rejects(t, TypeError(), response.text());
  }, 'Response constructed with an errored stream');

promise_test(t => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    stream.getReader();
    var response = new Response(stream);
    return promise_rejects(t, TypeError(), response.text());
  }, 'Response constructed with a locked stream');

promise_test(t => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    setTimeout(() => controller.enqueue(), 1);
    var response = new Response(stream);
    return promise_rejects(t, TypeError(), response.text());
  }, 'Response constructed stream with an undefined chunk');

promise_test(t => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    setTimeout(() => controller.enqueue(null), 1);
    var response = new Response(stream);
    return promise_rejects(t, TypeError(), response.text());
  }, 'Response constructed stream with a null chunk');

promise_test(t => {
    var controller;
    var stream = new ReadableStream({start: c => controller = c});
    setTimeout(() => controller.enqueue('hello'), 1);
    var response = new Response(stream);
    return promise_rejects(t, TypeError(), response.text());
  }, 'Response constructed stream with a string chunk');

done();
