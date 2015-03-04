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

// Method that behaves similarly to navigator.connect, but the actual connect
// call is made from a cross origin iframe.
function cross_origin_connect(t, service) {
  // |service| could be a relative URL, but for this to work from the iframe it
  // needs an absolute URL.
  var target_url = new URL(service, location.origin + base_path());
  return with_iframe(
      cross_origin + base_path() + 'resources/connect-helper.html')
    .then(function(iframe) {
        var channel = new MessageChannel();
        iframe.contentWindow.postMessage(
          {connect: target_url.href, port: channel.port2}, '*', [channel.port2]);
        return reply_as_promise(t, channel.port1);
    });
}

// Method that behaves similarly to navigator.connect, but the actual connect
// call is made from a worker.
function connect_from_worker(t, service) {
  // |service| is a relative URL, but for this to work from the worker it needs
  // an absolute URL.
  var target_url = location.origin + base_path() + service;
  var worker = new Worker('resources/connect-helper.js');
  var channel = new MessageChannel();
  worker.postMessage
    ({connect: target_url, port: channel.port2}, [channel.port2]);
  return reply_as_promise(t, channel.port1);
}
