// In tests that are not explicitly async, include this file to ensure
// paint completion before the layout test pixel results are captured.

if (window.testRunner)
    testRunner.waitUntilDone();

function finishTest() {
    if (window.testRunner) {
        testRunner.layoutAndPaintAsyncThen(function () { testRunner.notifyDone(); });
    }
}

window.addEventListener("load", finishTest, false);
