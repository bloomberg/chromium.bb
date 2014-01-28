// Tiny test rig for all security/link-crossorigin-*.html tests,
// which exercise <link> + CORS variations.
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
    var out = document.querySelector("pre");
    var txt = pass ? "PASS" : "FAIL";
    if (msg) txt += ": " + msg;
    out.appendChild(document.createTextNode(txt));
    out.appendChild(document.createElement("br"));
    if (++event_count >= test_count && window.testRunner)
        testRunner.notifyDone();
}

function pass() { finish(true); }
function fail() { finish(false); }
