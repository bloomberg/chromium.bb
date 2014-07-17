// Tiny test rig for all security/link-crossorigin-*.html tests,
// which exercise <link> + CORS variations.

self.jsTestIsAsync = true;

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

// The common case is to have four sub-tests. To override
// for a test, assign window.test_count.
var default_test_count = 4;

var event_count = 0;
test_count = window.test_count || default_test_count;

function finish(pass, msg) {
    if (pass)
        testPassed(msg || "");
     else
        testFailed(msg || "");
    if (++event_count >= test_count)
        finishJSTest();
}

function pass() { finish(true); }
function fail() { finish(false); }
