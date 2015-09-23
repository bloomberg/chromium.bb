if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

function readStream(reader, values) {
  reader.read().then(function(r) {
      if (!r.done) {
        values.push(r.value);
        readStream(reader, values);
      }
    });
  return reader.closed;
}

promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          // Accessing the body property makes the stream start working.
          var stream = response.body;
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '<!DOCTYPE html>\n');
        })
    }, 'FetchTextAfterAccessingStreamTest');

promise_test(function(test) {
    var chunks = [];
    var actual = '';
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          r = response;
          return readStream(response.body.getReader(), chunks);
        })
      .then(function() {
          var decoder = new TextDecoder();
          for (var chunk of chunks) {
            actual += decoder.decode(chunk, {stream: true});
          }
          // Put an empty buffer without the stream option to end decoding.
          actual += decoder.decode(new Uint8Array(0));
          assert_equals(actual, '<!DOCTYPE html>\n');
        })
    }, 'FetchStreamTest');

promise_test(function(test) {
    return fetch('/fetch/resources/progressive.php')
      .then(function(response) {
          var p1 = response.text();
          // Because progressive.php takes some time to load, we expect
          // response.text() is not yet completed here.
          var p2 = response.text().then(function() {
              return Promise.reject(new Error('resolved unexpectedly'));
            }, function(e) {
              return e;
            });
          return Promise.all([p1, p2]);
        })
      .then(function(results) {
          assert_equals(results[0].length, 190);
          assert_equals(results[1].name, 'TypeError');
        })
    }, 'FetchTwiceTest');

promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.arrayBuffer();
        })
      .then(function(b) {
          assert_equals(b.byteLength, 16);
        })
    }, 'ArrayBufferTest');

promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.blob();
        })
      .then(function(blob) {
          assert_equals(blob.size, 16);
          assert_equals(blob.type, 'text/html');
        })
    }, 'BlobTest');

promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.json();
        })
      .then(
        test.unreached_func('json() must fail'),
        function(e) {
          assert_equals(e.name, 'SyntaxError', 'expected JSON error');
        })
    }, 'JSONFailedTest');

promise_test(function(test) {
    return fetch('/fetch/resources/simple.json')
      .then(function(response) {
          return response.json();
        })
      .then(function(json) {
          assert_equals(json['a'], 1);
          assert_equals(json['b'], 2);
        })
    }, 'JSONTest');

promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '<!DOCTYPE html>\n');
        })
    }, 'TextTest');

promise_test(function(test) {
    return fetch('/fetch/resources/non-ascii.txt')
      .then(function(response) {
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '\u4e2d\u6587 Gem\u00fcse\n');
        })
    }, 'NonAsciiTextTest');

promise_test(function(test) {
    var expected = '';
    for (var i = 0; i < 100; ++i)
        expected += i;

    var decoder = new TextDecoder();
    var actual = '';
    var response;
    var reader;
    return fetch('/fetch/resources/progressive.php')
      .then(function(res) {
          response = res;
          reader = response.body.getReader();
          return reader.read();
        })
      .then(function(r) {
          assert_false(r.done);
          actual += decoder.decode(r.value, {stream: true});
        })
      .then(function() {
          return response.text().then(unreached_fulfillment(test), function() {
              // response.text() should fail because we have a reader.
            });
        })
      .then(function() {
          reader.releaseLock();
          return response.arrayBuffer();
        })
      .then(function(buffer) {
          actual += decoder.decode(buffer);
          assert_equals(actual, expected);
        })
    }, 'PartiallyReadFromStreamAndReadArrayBufferTest');

done();
