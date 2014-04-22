// Adapter for testharness.js-style tests with Service Workers

function service_worker_test(url, description) {
    var t = async_test(description);
    t.step(function() {

        navigator.serviceWorker.register(url).then(
            t.step_func(function(worker) {
                var messageChannel = new MessageChannel();
                messageChannel.port1.onmessage = t.step_func(onMessage);
                worker.postMessage({port:messageChannel.port2}, [messageChannel.port2]);
            }),
            t.step_func(function(reason) {
                assert_unreached('Registration should succeed, but failed: ' + reason.name);
            }));

        function onMessage(e) {
            assert_equals(e.data, 'pass');
            t.done();
        }
    });
}
