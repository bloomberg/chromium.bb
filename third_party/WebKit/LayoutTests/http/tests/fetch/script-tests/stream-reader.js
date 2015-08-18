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

promise_test(function(t) {
    return fetch('/fetch/resources/doctype.html').then(function(res) {
        var stream = res.body;
        var reader = stream.getReader();
        assert_throws({name: 'TypeError'}, function() { stream.getReader() });
        reader.releaseLock();
        var another = stream.getReader();
        assert_not_equals(another, reader);
      });
  }, 'ReadableStreamReader acquisition / releasing');

promise_test(function(t) {
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

promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        assert_false(res.bodyUsed);
        var reader = res.body.getReader();
        assert_true(res.bodyUsed);
        return res.text();
      }).then(unreached_rejection(t), function() {
        // text() should fail because bodyUsed is set.
      });
  }, 'acquiring a reader should set bodyUsed.');

promise_test(function(t) {
    var response;
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        response = res;
        assert_false(res.bodyUsed);
        var p = res.arrayBuffer();
        assert_true(res.bodyUsed);
        assert_throws({name: 'TypeError'}, function() { res.body.getReader() });
        return p;
      }).then(function(buffer) {
        assert_equals(buffer.byteLength, 190);
        // Now we can obtain a (closed) reader.
        return response.body.getReader().closed;
      });
  }, 'Setting bodyUsed means the body is locked.');

promise_test(function(t) {
    return fetch('/fetch/resources/slow-failure.cgi').then(function(res) {
        return res.text().then(function() {
            assert_unreached('text() should fail');
          }, function(e) {
            return res.body.getReader().closed.then(function() {
                assert_unreached('res.body should be errored');
              }, function(e) {});
          });
      });
   }, 'Error in text() should be propagated to the body stream.');

promise_test(function(t) {
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
        var original_body = original.body;
        var clone = original.clone();
        assert_not_equals(original.body, clone.body);
        assert_not_equals(original.body, original_body);
        assert_not_equals(clone.body, original_body);
        assert_throws({name: 'TypeError'}, function() {
            original_body.getReader();
        });
        var reader1 = original.body.getReader();
        var reader2 = clone.body.getReader();
        return Promise.all([read_until_end(reader1), read_until_end(reader2)]);
      }).then(function(r) {
        var read1 = 0;
        var read2 = 0;
        for (var chunk of r[0]) {
          read1 += chunk.byteLength;
        }
        for (var chunk of r[1]) {
          read2 += chunk.byteLength;
        }
        // Make sure that we received all data in total.
        assert_equals(read + read1, 190);
        assert_equals(read + read2, 190);
      });
  }, 'Clone after reading partially');

promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        res.body.cancel();
        return res.text();
      }).then(function(text) {
        assert_equals(text, '');
      });
  }, 'Cancelling stream stops downloading.');

promise_test(function(t) {
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        var clone = res.clone();
        res.body.cancel();
        return Promise.all([res.arrayBuffer(), clone.arrayBuffer()]);
      }).then(function(r) {
        assert_equals(r[0].byteLength, 0);
        assert_equals(r[1].byteLength, 190);
      });
  }, 'Cancelling stream should not affect cloned one.');

promise_test(function(t) {
    var stream;
    return fetch('/fetch/resources/progressive.php').then(function(res) {
        var p = res.text();
        stream = res.body;
        assert_throws({name: 'TypeError'}, function() { stream.getReader() });
        return p;
      }).then(function(text) {
        assert_equals(text.length, 190);
        return stream.getReader().read();
      }).then(function(r) {
        assert_true(r.done);
        assert_equals(r.value, undefined);
      });
  }, 'Accessing body when processing text().');

done();
