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

function readStream(stream, values) {
  while (stream.state === 'readable') {
    values.push(stream.read());
  }
  if (stream.state === 'waiting') {
    return stream.wait().then(function() {
        readStream(stream, values);
      });
  }
  return stream.closed;
}

promise_test(function(test) {
    var response;
    return fetch('/fetch/resources/doctype.html')
      .then(function(resp) {
          response = resp;
          return response.body.wait();
        })
      .then(function() {
          if (response.body.state !== 'readable') {
            return Promise.reject(TypeError('stream state get wrong'));
          } else {
            return response.text();
          }
        })
      .then(function(text) {
          assert_equals(text, '<!DOCTYPE html>\n', 'response.body');
        })
    }, 'FetchTextAfterStreamGetReadableTest');

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
    var values = [];
    return fetch('/fetch/resources/doctype.html')
      .then(function(response) {
          r = response;
          return readStream(response.body, values);
        })
      .then(function() {
          return Promise.all(values.map(arrayBufferToString));
        })
      .then(function(strings) {
          var string = String.prototype.concat.apply('', strings);
          assert_equals(string, '<!DOCTYPE html>\n');
        })
    }, 'FetchStreamTest');

promise_test(function(test) {
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
    return fetch('/serviceworker/resources/simple.json')
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

done();
