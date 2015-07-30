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
// Fetch tests
var SuboriginFetchTest = function(pass, name, src, crossoriginValue) {
    SuboriginTest.call(this, pass, "Fetch: " + name, src, crossoriginValue);
}

SuboriginFetchTest.prototype.execute = function() {
    var test = async_test(this.name);
    var options = {};

    if (this.crossoriginValue === 'same-origin') {
        options.mode = 'same-origin';
    } else if (this.crossoriginValue === 'anonymous') {
        options.mode = 'cors';
    } else if (this.crossoriginValue === 'use-credentials') {
        options.mode = 'cors';
        options.credentials = 'include';
    }

    var pass = this.pass;
    fetch(this.src, options)
        .then(function(response) {
            if (pass) {
                test.step(function() { test.done(); });
            } else {
                test.step(function() { assert_unreached("Bad Fetch succeeded."); test.done(); });
            }
        })
        .catch(function(error) {
            if (pass) {
                test.step(function() { assert_unreached("Good Fetch failed."); test.done(); });
            } else {
                test.step(function() { test.done(); });
            }
        });
};

// Fetch tests
new SuboriginFetchTest(
    false,
    "anonymous, ACAO: " + server,
    xoriginAnonScript(),
    "anonymous").execute();

new SuboriginFetchTest(
    true,
    "anonymous, ACAO: *",
    xoriginAnonScript('*'),
    "anonymous").execute();

new SuboriginFetchTest(
    false,
    "use-credentials, ACAO: " + server,
    xoriginCredsScript(),
    "use-credentials").execute();

new SuboriginFetchTest(
    false,
    "anonymous, CORS-ineligible resource",
    xoriginIneligibleScript(),
    "anonymous").execute();

new SuboriginFetchTest(
    false,
    "same-origin, ACAO: * ",
    xoriginAnonScript('*'),
    "same-origin").execute();
</script>
</body>
</html>
