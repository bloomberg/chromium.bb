// Common helper functions for foreign fetch tests.

// Installs a service worker on a different origin. Both |worker| and |scope|
// are resolved relative to the /serviceworker/resources/ directory on a
// remote origin.
function install_cross_origin_worker(t, worker, scope) {
  return with_iframe(get_host_info().HTTPS_REMOTE_ORIGIN +
                     '/serviceworker/resources/install-worker-helper.html')
    .then(frame => new Promise((resolve, reject) => {
        var channel = new MessageChannel();
        frame.contentWindow.postMessage({worker: worker,
                                         options: {scope: scope},
                                         port: channel.port1},
                                        '*', [channel.port1]);
        channel.port2.onmessage = reply => {
            if (reply.data == 'success') resolve();
            else reject(reply.data);
          };
      }));
}
