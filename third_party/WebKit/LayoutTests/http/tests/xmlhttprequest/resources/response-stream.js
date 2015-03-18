var global = this;
if (global.importScripts) {
    // Worker case
    importScripts('/resources/testharness.js');
}

var test = async_test('Test response of XMLHttpRequest with responseType set to "stream" for various readyState.');

test.step(function() {
    var xhr = new XMLHttpRequest;

    xhr.responseType = 'stream';
    assert_equals(xhr.responseType, 'stream', 'xhr.responseType');

    assert_equals(xhr.readyState, xhr.UNSENT, 'xhr.readyState');
    assert_equals(xhr.response, null, 'xhr.response during UNSENT');

    var seenStates = [];

    function readStream(reader) {
        var chunks = [];
        function rec(resolve, reject) {
            while (reader.state === 'readable') {
                chunks.push(reader.read());
            }
            if (reader.state === 'closed') {
                resolve(chunks);
                return;
            }
            reader.ready.then(function() {
                rec(resolve, reject);
            }).catch(reject);
        }
        return new Promise(rec);
    }
    var streamPromise = undefined;

    xhr.onreadystatechange = test.step_func(function() {
        // onreadystatechange can be invoked multiple times in LOADING state.
        if (seenStates.length == 0 || xhr.readyState != seenStates[seenStates.length - 1])
            seenStates.push(xhr.readyState);

        switch (xhr.readyState) {
        case xhr.UNSENT:
            assert_unreached('Unexpected readyState: UNSENT');
            return;

        case xhr.OPENED:
            assert_equals(xhr.response, null, 'xhr.response during OPENED');
            return;

        case xhr.HEADERS_RECEIVED:
            assert_equals(xhr.response, null, 'xhr.response during HEADERS_RECEIVED');
            return;

        case xhr.LOADING:
            assert_not_equals(xhr.response, null, 'xhr.response during LOADING');
            assert_true(xhr.response instanceof ReadableStream,
                'xhr.response should be ReadableStream during LOADING');
            if (streamPromise === undefined) {
                streamPromise = readStream(xhr.response.getReader());
            }
            streamPromise.then(function(chunks) {
                assert_equals(xhr.status, 200, 'xhr.status');

                // Check that we saw all states.
                assert_array_equals(seenStates,
                    [xhr.OPENED, xhr.HEADERS_RECEIVED, xhr.LOADING, xhr.DONE]);
                var size = 0;
                for (var i = 0; i < chunks.length; ++i) {
                    size += chunks[i].byteLength;
                }
                assert_equals(size, 103746, 'response size');
                return xhr.response.closed;
            }).then(function() {
                test.done();
            }).catch(test.step_func(function(e) {
                throw e;
            }));
            return;

        case xhr.DONE:
            return;

        default:
            assert_unreached('Unexpected readyState: ' + xhr.readyState)
            return;
        }
    });

    xhr.open('GET', '/resources/test.ogv', true);
    xhr.send();
});

if (global.importScripts) {
    // Worker case
    done();
}
