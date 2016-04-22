if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
  importScripts('/streams/resources/rs-utils.js');
}

promise_test(t => {
    return fetch('/fetch/resources/doctype.html').then(res => {
        var stream = res.v8ExtraStreamBody();
        assert_equals(stream.constructor, ReadableStream, 'stream type');
        var reader = stream.getReader();
        return readableStreamToArray(stream, reader);
      }).then(chunks => {
        var decoder = new TextDecoder();
        var result = '';
        for (var chunk of chunks) {
          assert_equals(chunk.constructor, Uint8Array, 'chunk type');
          result += decoder.decode(chunk, {stream: true});
        }
        result += decoder.decode(new Uint8Array(0), {stream: false});
        assert_equals(result, '<!DOCTYPE html>\n');
      });
  }, 'read contents via v8ExtraStreamBody');

done();
