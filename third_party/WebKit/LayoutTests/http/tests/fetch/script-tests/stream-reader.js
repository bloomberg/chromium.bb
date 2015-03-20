if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
}

function read_until_end(reader) {
  var chunks = [];
  function consume() {
    return reader.read().then(function(r) {
        if (r.done) {
          return chunks;
        } else {
          chunks.push(r.value);
          return consume();
        }
      });
  }
  return consume();
}

sequential_promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var stream = res.body;
        var reader = stream.getReader();
        assert_throws({name: 'TypeError'}, function() { stream.getReader() });
        reader.releaseLock();
        var another = stream.getReader();
        assert_not_equals(another, reader);
      });
  }, 'ReadableStreamReader acquisition / releasing');

sequential_promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var reader = res.body.getReader();
        return read_until_end(reader);
      }).then(function(chunks) {
        var size = 0;
        for (var chunk of chunks) {
          assert_equals(chunk.constructor, Uint8Array, 'chunk\'s type');
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
  }, 'read contents with ReadableStreamReader');

sequential_promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        assert_false(res.bodyUsed);
        var reader = res.body.getReader();
        assert_true(res.bodyUsed);
        return res;
      }).then(function(res) {
        return res.text();
      }).then(unreached_rejection(t), function() {
        // text() should fail because bodyUsed is set.
      });
  }, 'acquiring a reader should set bodyUsed.');

sequential_promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        // We need to access body attribute to start the stream.
        res.body;
        assert_false(res.bodyUsed);
        res.text();
        assert_true(res.bodyUsed);
        // FIXME: Getting a reader should throw, but it doesn't because the
        // current implementation closes the body.
        // assert_throws({name: 'TypeError'}, function() { res.body.getReader() });
      });
  }, 'Setting bodyUsed means the body is locked.');

sequential_promise_test(function(t) {
    var reader;
    var read = 0;
    var original;
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        original = res;
        reader = res.body.getReader();
        return reader.read();
      }).then(function(r) {
        assert_false(r.done);
        read += r.value.byteLength;
        // Make sure that we received something but we didn't receive all.
        assert_not_equals(read, 0);
        assert_not_equals(read, 190);

        reader.releaseLock();
        return read_until_end(original.clone().body.getReader());
      }).then(function(chunks) {
        for (var chunk of chunks) {
          read += chunk.byteLength;
        }
        // Make sure that we received all data in total.
        assert_equals(read, 190);
      });
  }, 'Clone after reading partially');

sequential_promise_test_done();
done();
