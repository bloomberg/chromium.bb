self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    var port = message.port;
    function bind(test) {
      return function() {
        return test(port).catch(function(e) {
            port.postMessage('Error: ' + e);
         });
      };
    }
    doTextTest(port)
      .then(bind(doJSONTest))
      .then(bind(doJSONFailedTest))
      .then(bind(doBlobTest))
      .then(bind(doArrayBufferTest))
      .then(bind(doFetchTwiceTest))
      .then(bind(doFetchStreamTest))
      .then(bind(doFetchTextAfterAccessingStreamTest))
      .then(bind(doFetchTextAfterStreamGetReadableTest))
      .then(bind(quit))
      .catch(bind(quit))
  }
};

function quit(port) {
  port.postMessage('quit');
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

function doFetchTextAfterStreamGetReadableTest(port) {
  var response;
  return fetch('doctype.html')
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
        port.postMessage('Text: ' + text);
      });
}

function doFetchTextAfterAccessingStreamTest(port) {
  return fetch('doctype.html')
    .then(function(response) {
        // Accessing the body property makes the stream start working.
        var stream = response.body;
        return response.text();
      })
    .then(function(text) {
        port.postMessage('Text: ' + text);
      });
}

function doFetchStreamTest(port) {
  var values = [];
  return fetch('doctype.html')
    .then(function(response) {
        r = response;
        return readStream(response.body, values);
      })
    .then(function() {
        return Promise.all(values.map(arrayBufferToString));
      })
    .then(function(strings) {
        var string = String.prototype.concat.apply('', strings);
        port.postMessage('stream: ' + string);
      });
}

function doFetchTwiceTest(port) {
  return fetch('doctype.html')
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
        port.postMessage(results[0] + ' : ' + results[1].name);
      });
}

function doArrayBufferTest(port) {
  return fetch('doctype.html')
    .then(function(response) {
        return response.arrayBuffer();
      })
    .then(function(b) {
        port.postMessage('ArrayBuffer: ' + b.byteLength);
      });
}

function doBlobTest(port) {
  return fetch('doctype.html')
    .then(function(response) {
        return response.blob();
      })
    .then(function(blob) {
        port.postMessage('Blob: ' + blob.size + ' : ' + blob.type);
      });
}

function doJSONFailedTest(port) {
  return fetch('doctype.html')
    .then(function(response) {
        return response.json();
      })
    .then(function(){}, function(e) {
        port.postMessage('JSON: ' + e.name);
      });
}

function doJSONTest(port) {
  return fetch('simple.json')
    .then(function(response) {
        return response.json();
      })
    .then(function(json) {
        port.postMessage('JSON: ' + json['a'] + ' ' + json['b']);
      });
}

function doTextTest(port) {
  return fetch('doctype.html')
    .then(function(response) {
        return response.text();
      })
    .then(function(txt) {
        port.postMessage('Text: ' + txt);
      });
}
