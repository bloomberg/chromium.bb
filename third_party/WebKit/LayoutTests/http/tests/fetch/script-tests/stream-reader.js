if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
}

function read_until_end(reader) {
  var chunks = [];
  function rec(resolve, reject) {
    while (reader.state === 'readable') {
      chunks.push(reader.read());
    }
    if (reader.state === 'closed') {
      resolve(chunks);
      return;
    }
    if (reader.state === 'errored') {
      resolve(reader.closed);
    }
    reader.ready.then(function() {
        rec(resolve, reject);
      }).catch(reject);
  }
  return new Promise(rec);
}

promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var stream = res.body;
        var reader = stream.getReader();
        assert_true(reader.isActive);
        assert_throws({name: 'TypeError'}, function() { stream.getReader() });
        reader.releaseLock();
        var another = stream.getReader();
        assert_not_equals(another, reader);
        assert_false(reader.isActive);
        assert_true(another.isActive);
      });
  }, 'ExclusiveStreamReader acquisition / releasing');

promise_test(function(t) {
    function wait_until_readable(reader) {
      return reader.ready.then(function() {
          if (reader.state === 'waiting') {
            return wait_until_readable(reader);
          }
          if (reader.state === 'readable') {
            return undefined;
          }
          return Promise.reject(new Error('state = ' + reader.state));
        });
    }
    var stream;
    var reader;
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        stream = res.body;
        reader = stream.getReader();
        return wait_until_readable(reader);
      }).then(function() {
        assert_equals(reader.state, 'readable');
        assert_equals(stream.state, 'waiting');
        reader.releaseLock();
        assert_equals(reader.state, 'closed');
        assert_equals(stream.state, 'readable');
        var another = stream.getReader();
        assert_equals(reader.state, 'closed');
        assert_equals(stream.state, 'waiting');
        assert_equals(another.state, 'readable');
      });
  }, 'ExclusiveStreamReader state masking');

promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var reader = res.body.getReader();
        return read_until_end(reader);
      }).then(function(chunks) {
        var size = 0;
        for (var chunk of chunks) {
          size += chunk.byteLength;
        }
        var buffer = new Uint8Array(size);
        var offset = 0;
        for (var chunk of chunks) {
          buffer.set(new Uint8Array(chunk), offset);
          offset += chunk.byteLength;
        }
        return new TextDecoder().decode(buffer);
      }).then(function(string) {
          assert_equals(string, '<!DOCTYPE html>\n');
      });
  }, 'read contents with ExclusiveStreamReader');

done();
