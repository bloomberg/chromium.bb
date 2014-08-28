self.onmessage = function(e) {
    var message = e.data;
    if ('port' in message) {
        port = message.port;
        doTextTest(port);
    }
};

function quit(port) {
    port.postMessage('quit');
    self.close();
}

function doFetchTwiceTest(port) {
  var p1Out = p2Out = null;

  fetch('doctype.html')
  .then(function(response) {
      var p1 = response.body.asText();
      var p2 = response.body.asText();

      p1.then(function(obj) {
          p1Out = obj;
          if (p2Out) {
              complete();
          }
      });
      p2.catch(function(e) {
          p2Out = e;
          if (p1Out) {
              complete();
          }
      });
  });

  function complete() {
      port.postMessage(p1Out + ' : ' + p2Out.name);
      quit(port);
  }
}

function doArrayBufferTest(port) {
    fetch('doctype.html')
    .then(function(response) {
        response.body.asArrayBuffer()
        .then(function(b) {
            port.postMessage('ArrayBuffer: ' + b.byteLength);
            doFetchTwiceTest(port);
        });
    });
}

function doBlobTest(port) {
    fetch('doctype.html')
    .then(function(response) {
        response.body.asBlob()
        .then(function(blob) {
            port.postMessage('Blob: ' + blob.size + " : " + blob.type);
            doArrayBufferTest(port);
        });
    });
}

function doJSONFailedTest(port) {
  fetch('doctype.html')
  .then(function(response) {
      response.body.asJSON()
      .catch(function(e) {
          port.postMessage('JSON: ' + e.name);
          doBlobTest(port);
      });
  });
}

function doJSONTest(port) {
    fetch('simple.json')
    .then(function(response) {
        response.body.asJSON()
        .then(function(json) {
            port.postMessage('JSON: ' + json['a'] + ' ' + json['b']);
            doJSONFailedTest(port);
        });
    });
}

function doTextTest(port) {
  fetch('doctype.html')
  .then(function(response) {
      response.body.asText()
      .then(function(txt) {
          port.postMessage('Text: ' + txt);
          doJSONTest(port);
      });
  });
}