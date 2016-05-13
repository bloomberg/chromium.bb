description("Tests that when a Geolocation request is made from a remote frame, and that frame's script context goes away before the Geolocation callback is made, the callback is made as usual.");

function onFirstIframeLoaded() {
    iframe.src = 'resources/callback-to-deleted-context-inner2.html';
}

function onSecondIframeLoaded() {
    window.setTimeout(function() {
        testFailed('No callbacks invoked');
        finishJSTest();
    }, 500);
}

var iframe = document.createElement('iframe');
geolocationServiceMock.then(mock => {
    mock.setGeolocationPermission(true);
    mock.setGeolocationPosition(51.478, -0.166, 100);
    iframe.src = 'resources/callback-to-deleted-context-inner1.html';
    document.body.appendChild(iframe);
});

window.jsTestIsAsync = true;
