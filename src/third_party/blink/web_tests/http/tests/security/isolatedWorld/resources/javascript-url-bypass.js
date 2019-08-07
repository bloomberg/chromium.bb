if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

// This is needed because isolated worlds are not reset between test runs and a
// previous test's CSP may interfere with this test. See
// https://crbug.com/415845.
testRunner.setIsolatedWorldInfo(1, null, null);

tests = 1;
window.addEventListener("message", function(message) {
    tests -= 1;
    test();
}, false);

function setup() {

    var iframe = document.getElementById('testiframe');
    iframe.onload = function () {
        alert('iframe loaded');
        window.postMessage("next", "*");
    }

    test();
}

function test() {
    function setIframeSrcToJavaScript(num) {
        var iframe = document.getElementById('testiframe');
        // Provide a body in the iframe src to trigger an onload event once
        // execution has finished.
        iframe.src = "javascript:alert('iframe javascript: src running') || 'alerted'";
    }

    alert("Running test #" + tests + "\n");
    switch (tests) {
        case 1:
            alert("Bypass main world's CSP with javascript: URL");
            testRunner.setIsolatedWorldInfo(1, "chrome-extension://123", "frame-src *; script-src 'unsafe-inline'");
            testRunner.evaluateScriptInIsolatedWorld(1, String(eval("setIframeSrcToJavaScript")) + "\nsetIframeSrcToJavaScript(1);");
            break;
        case 0:
            testRunner.notifyDone();
            break;
    }
}

document.addEventListener('DOMContentLoaded', setup);
