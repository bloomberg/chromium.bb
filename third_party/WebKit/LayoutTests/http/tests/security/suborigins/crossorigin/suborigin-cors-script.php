<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Allow suborigin in HTTP header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="/security/suborigins/resources/suborigin-cors-lib.js"></script>
</head>
<body>
<div id="container"></div>
<script>
var SuboriginScriptTest = function(pass, name, src, crossoriginValue) {
    SuboriginTest.call(this, pass, "Script: " + name, src, crossoriginValue);
}

SuboriginScriptTest.prototype.execute = function() {
    var test = async_test(this.name);
    var e = document.createElement("script");
    e.src = this.src;
    if (this.crossoriginValue) {
        e.setAttribute("crossorigin", this.crossoriginValue);
    }
    if (this.pass) {
        e.addEventListener("load", function() { test.done(); });
        e.addEventListener("error", function() {
            test.step(function() { assert_unreached("Good load fired error handler."); });
        });
    } else {
        e.addEventListener("load", function() {
            test.step(function() { assert_unreached("Bad load successful.") });
        });
        e.addEventListener("error", function() { test.done(); });
    }
    document.body.appendChild(e);
}

// This is unused but required by cors-script.php.
window.result = false;

// Script tests
new SuboriginScriptTest(
    false,
    "<crossorigin='anonymous'>, ACAO: " + server,
    xoriginAnonScript(),
    "anonymous").execute();

new SuboriginScriptTest(
    true,
    "<crossorigin='anonymous'>, ACAO: *",
    xoriginAnonScript('*'),
    "anonymous").execute();

new SuboriginScriptTest(
    false,
    "<crossorigin='use-credentials'>, ACAO: " + server,
    xoriginCredsScript(),
    "use-credentials").execute();

new SuboriginScriptTest(
    false,
    "<crossorigin='anonymous'>, CORS-ineligible resource",
    xoriginIneligibleScript(),
    "anonymous").execute();
</script>
</body>
</html>
