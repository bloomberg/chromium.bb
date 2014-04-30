description('Tests that a cached position can be obtained in one frame after another frame has received a fresh position.');

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPosition(document, 51.478, -0.166, 100);
internals.setGeolocationPermission(document, true);

window.onmessage = function (messageEvent) {
    debug(messageEvent.data.message);
    success = messageEvent.data.success;
    shouldBeTrue('success');
    finishJSTest();
}

navigator.geolocation.getCurrentPosition(
    function() {
        // Kick off the iframe to request a cached position. The iframe
        // will post a message back on success / failure.
        iframe = document.createElement('iframe');
        iframe.src = 'resources/cached-position-iframe-inner.html';
        document.body.appendChild(iframe);
    });

window.jsTestIsAsync = true;
