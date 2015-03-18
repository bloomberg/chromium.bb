var global = this;
if (global.importScripts) {
    // Worker case
    importScripts('/resources/testharness.js');
}

var testInLoadingState = async_test('Test aborting XMLHttpRequest with responseType set to "stream" in LOADING state.');

testInLoadingState.step(function()
{
    var xhr = new XMLHttpRequest;

    xhr.responseType = 'stream';

    var seenStates = [];

    xhr.onreadystatechange = testInLoadingState.step_func(function() {
        // onreadystatechange can be invoked multiple times in LOADING state.
        if (seenStates.length == 0 || xhr.readyState != seenStates[seenStates.length - 1])
            seenStates.push(xhr.readyState);

        switch (xhr.readyState) {
        case xhr.UNSENT:
            assert_unreached('Unexpected readyState: UNSENT');
            return;

        case xhr.OPENED:
        case xhr.HEADERS_RECEIVED:
            return;

        case xhr.LOADING:
            var stream = xhr.response;
            assert_true(stream instanceof ReadableStream, 'xhr.response shoud be ReadableStream');
            assert_array_equals(seenStates, [xhr.OPENED, xhr.HEADERS_RECEIVED, xhr.LOADING]);

            xhr.abort();

            assert_equals(xhr.readyState, xhr.UNSENT, 'xhr.readyState after abort() call');
            assert_equals(xhr.response, null, 'xhr.response after abort() call');
            assert_array_equals(seenStates, [xhr.OPENED, xhr.HEADERS_RECEIVED, xhr.LOADING, xhr.DONE]);
            stream.closed.then(testInLoadingState.step_func(assert_unreached), testInLoadingState.done.bind(testInLoadingState));
            return;

        case xhr.DONE:
            return;

        default:
            assert_unreached('Unexpected readyState: ' + xhr.readyState);
            return;
        }
    });

    xhr.open('GET', '/resources/test.ogv', true);
    xhr.send();
});

var testInDoneState = async_test('Test aborting XMLHttpRequest with responseType set to "stream" in DONE state.');

function readUntilDone(reader) {
    return reader.ready.then(function() {
        while (reader.state == 'readable') {
            reader.read();
        }
        if (reader.state == 'closed' || reader.state == 'errored') {
            return reader.closed;
        } else {
            return readUntilDone(reader);
        }
    });
}

testInDoneState.step(function()
{
    var xhr = new XMLHttpRequest;

    xhr.responseType = 'stream';

    var seenStates = [];
    var stream;

    xhr.onreadystatechange = testInDoneState.step_func(function() {
        // onreadystatechange can be invoked multiple times in LOADING state.
        if (seenStates.length == 0 || xhr.readyState != seenStates[seenStates.length - 1])
            seenStates.push(xhr.readyState);

        switch (xhr.readyState) {
        case xhr.UNSENT:
        case xhr.OPENED:
        case xhr.HEADERS_RECEIVED:
            return;
        case xhr.LOADING:
            if (!stream) {
                stream = xhr.response;
                assert_true(stream instanceof ReadableStream, 'xhr.response shoud be ReadableStream');
                readUntilDone(stream.getReader()).then(function() {
                    assert_true(stream instanceof ReadableStream, 'xhr.response should be ReadableStream');
                    assert_equals(xhr.status, 200, 'xhr.status');
                    assert_not_equals(xhr.response, null, 'xhr.response during DONE');

                    xhr.abort();

                    assert_equals(xhr.readyState, xhr.UNSENT, 'xhr.readyState after abort() call');
                    assert_equals(xhr.response, null, 'xhr.response after abort() call');

                    assert_array_equals(seenStates, [xhr.OPENED, xhr.HEADERS_RECEIVED, xhr.LOADING, xhr.DONE]);
                    return stream.closed;
                }).then(function() {
                    testInDoneState.done();
                }).catch(testInDoneState.step_func(function(e) {
                    assert_unreached(e);
                }));
            }
            return;

        case xhr.DONE:
            return;

        default:
            assert_unreached('Unexpected readyState: ' + xhr.readyState);
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
