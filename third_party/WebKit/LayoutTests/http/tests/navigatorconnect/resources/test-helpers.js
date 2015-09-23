// Helper method that waits for a {success: <boolean>, result: any} reply on
// a port and returns a promise that resolves (if success is true) or rejects
// the promise with the value of the result attribute.
function reply_as_promise(t, port) {
  return new Promise(function(resolve, reject) {
      var got_reply = false;
      port.onmessage = t.step_func(function(event) {
        assert_false(got_reply);
        assert_true('success' in event.data);
        assert_true('result' in event.data);
        got_reply = true;
        if (event.data.success)
          resolve(event.data.result);
        else
          reject(event.data.result);
      });
    });
}

// Method that behaves similarly to navigator.services.connect, but the actual
// connect call is made from a cross origin iframe. Also the returned port is a
// MessagePort instead of a ServicePort, but with targetURL, name and data
// attributes set.
function cross_origin_connect(t, service, options) {
  // |service| could be a relative URL, but for this to work from the iframe it
  // needs an absolute URL.
  var target_url = new URL(service, location.origin + base_path());
  return with_iframe(
      cross_origin + base_path() + 'resources/connect-helper.html')
    .then(function(iframe) {
        var channel = new MessageChannel();
        iframe.contentWindow.postMessage(
          {connect: target_url.href, port: channel.port2, options: options}, '*', [channel.port2]);
        return reply_as_promise(t, channel.port1);
      })
    .then(function(result) {
        var port = result.port;
        port.targetURL = result.targetURL;
        port.name = result.name;
        port.data = result.data;
        return port;
      });
}

// Method that behaves similarly to navigator.connect, but the actual connect
// call is made from a worker. Also the returned port is a MessagePort instead
// of a ServicePort, but with targetURL, name and data attributes set.
function connect_from_worker(t, service, options) {
  // |service| is a relative URL, but for this to work from the worker it needs
  // an absolute URL.
  var target_url = location.origin + base_path() + service;
  var worker = new Worker('resources/connect-helper.js');
  var channel = new MessageChannel();
  worker.postMessage
    ({connect: target_url, port: channel.port2, options: options}, [channel.port2]);
  return reply_as_promise(t, channel.port1)
    .then(function(result) {
        var port = result.port;
        port.targetURL = result.targetURL;
        port.name = result.name;
        port.data = result.data;
        return port;
      });
}

// Takes (a promise resolving to) a ServicePort instance, and returns a Promise
// that resolves to a MessagePort wrapping that ServicePort. Used to simplify
// testing code and to allow forwarding a connection from a cross origin iframe
// or worker to the main test runner.
function wrap_in_port(maybe_port) {
  return Promise.resolve(maybe_port).then(
    function(port) {
      var channel = new MessageChannel();
      channel.port2.onmessage = function(event) {
        port.postMessage(event.data, event.ports);
      };
      // Should use addEventListener and check source of event, but source isn't
      // set yet, so for now just assume only one wrapped port is used at a time.
      navigator.services.onmessage = function(event) {
        channel.port2.postMessage(event.data, event.ports);
      };
      channel.port1.targetURL = port.targetURL;
      channel.port1.name = port.name;
      channel.port1.data = port.data;
      return channel.port1;
    }
  );
}

