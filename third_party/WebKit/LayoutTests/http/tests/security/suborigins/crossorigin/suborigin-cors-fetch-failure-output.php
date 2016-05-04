<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<body>
<script>
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}
console.log("If a Suborigin makes a request, a response without an Access-Control-Allow-Suborigin header should fail and should output a reasonable error message.");

function success() {
    console.log("PASS: Fetch correctly failed");
    next();
}

function failure() {
    console.log("FAIL: Fetch incorrectly succeeded");
    next();
}

// First one should fail with preflight failure. Second one should
// fail with access control header failure.
var tests = [
    function() {
        var headers = new Headers();
        headers.append("x-custom-header", "foobar");
        var options = {
            headers: headers
        };
        fetch("http://127.0.0.1:8000/security/resources/cors-script.php?cors=false", options)
            .then(failure)
            .catch(success);
    },
    function() {
        fetch("http://127.0.0.1:8000/security/resources/cors-script.php?cors=false")
            .then(failure)
            .catch(success);
    }
];

function next() {
    if (tests.length !== 0) {
        tests.shift()();
    } else if (window.testRunner) {
        testRunner.notifyDone();
    }
}

next();
</script>
</body>
</html>
