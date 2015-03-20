if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

function arrayBufferToString(buffer) {
  return new Promise(function(resolve) {
      var reader = new FileReader();
      reader.onload = function() {
        resolve(reader.result);
      };
      reader.readAsText(new Blob([buffer]));
  });
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

sequential_promise_test(function(test) {
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

sequential_promise_test(function(test) {
    var values = [];
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          r = response;
          return readStream(response.body.getReader(), values);
        })
      .then(function() {
          return Promise.all(values.map(arrayBufferToString));
        })
      .then(function(strings) {
          var string = String.prototype.concat.apply('', strings);
          assert_equals(string, '<!DOCTYPE html>\n');
        })
    }, 'FetchStreamTest');

sequential_promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          var p1 = response.text();
          var p2 = response.text().then(function() {
              return Promise.reject(new Error('resolved unexpectedly'));
            }, function(e) {
              return e;
            });
          return Promise.all([p1, p2]);
        })
      .then(function(results) {
          assert_equals(results[0], '<!DOCTYPE html>\n');
          assert_equals(results[1].name, 'TypeError');
        })
    }, 'FetchTwiceTest');

sequential_promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.arrayBuffer();
        })
      .then(function(b) {
          assert_equals(b.byteLength, 16);
        })
    }, 'ArrayBufferTest');

sequential_promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.blob();
        })
      .then(function(blob) {
          assert_equals(blob.size, 16);
          assert_equals(blob.type, 'text/html');
        })
    }, 'BlobTest');

sequential_promise_test(function(test) {
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

sequential_promise_test(function(test) {
    return fetch('/serviceworker/resources/simple.json')
      .then(function(response) {
          return response.json();
        })
      .then(function(json) {
          assert_equals(json['a'], 1);
          assert_equals(json['b'], 2);
        })
    }, 'JSONTest');

sequential_promise_test(function(test) {
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          return response.text();
        })
      .then(function(text) {
          assert_equals(text, '<!DOCTYPE html>\n');
        })
    }, 'TextTest');

sequential_promise_test(function(test) {
    var expectedText = '';
    for (var i = 0; i < 100; ++i)
        expectedText += i;

    var values = [];
    function partialReadResponse(reader, read_count) {
      return Promise.resolve().then(function() {
          var promise = Promise.resolve();
          for (var i = 0; i < read_count; ++i) {
              promise = reader.read().then(function(r) {
                  if (!r.done) {
                    values.push(r.value);
                  }
                });
          }
          return promise;
        });
    }
    var response;
    var reader;
    return fetch('/fetch/resources/progressive.php')
      .then(function(res) {
          response = res;
          reader = response.body.getReader();
          return partialReadResponse(reader, 10);
        })
      .then(function() {
          return response.text().then(unreached_fulfillment(test), function() {
              // response.text() should fail because we have a reader.
            });
        })
      .then(function() {
          reader.releaseLock();
          return Promise.all(
              values.map(arrayBufferToString).concat(response.text()));
        })
      .then(function(strings) {
          var string = String.prototype.concat.apply('', strings);
          assert_equals(string, expectedText);
        })
    }, 'PartiallyReadFromStreamAndReadTextTest');

sequential_promise_test_done();
done();
