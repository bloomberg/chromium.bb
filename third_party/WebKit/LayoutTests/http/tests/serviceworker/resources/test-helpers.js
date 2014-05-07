// Adapter for testharness.js-style tests with Service Workers

function service_worker_test(url, description) {
    var t = async_test(description);
    t.step(function() {

        navigator.serviceWorker.register(url, {scope:'nonexistent'}).then(
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

// FIXME: Replace this with test.unreached_func(desc) once testharness.js is updated
// Use with unexpected event handlers or Promise rejection.
// E.g.:
//    onbadevent = fail(t, 'Should only see good events');
//    Promise.then(...).catch(fail(t, 'Rejection is never fun'));
function unreached_func(test, desc) {
    return test.step_func(function() {
        assert_unreached(desc);
    });
}

// Rejection-specific helper that provides more details
function unreached_rejection(test, prefix) {
    return test.step_func(function(reason) {
        assert_unreached(prefix + ': ' + reason.name);
    });
}
