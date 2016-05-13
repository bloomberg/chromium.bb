description("Tests for a crash when clearWatch() is called with a zero ID.<br><br>We call clearWatch() with a request in progress then navigate the page. This accesses the watchers map during cleanup and triggers the crash. This page should not be visible when the test completes.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

document.body.onload = function() {
    geolocationServiceMock.then(mock => {
        mock.setGeolocationPermission(true);
        mock.setGeolocationPosition(51.478, -0.166, 100);

        navigator.geolocation.watchPosition(function() {});
        navigator.geolocation.clearWatch(0);
        location = "data:text/html,TEST COMPLETE<script>if(window.testRunner) testRunner.notifyDone();</script>";
    });
}

window.jsTestIsAsync = true;
