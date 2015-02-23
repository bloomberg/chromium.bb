function displayThenCall(callback)
{
    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.displayAsyncThen(function() { callback(); testRunner.notifyDone(); });
    } else {
        setTimeout(callback, 500);
    }
}
