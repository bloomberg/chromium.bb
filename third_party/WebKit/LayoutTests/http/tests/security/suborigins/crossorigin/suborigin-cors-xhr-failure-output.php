<?php
header("Content-Security-Policy: suborigin foobar");
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
    alert("PASS: XHR correctly failed");
    next();
}

function failure() {
    alert("FAIL: XHR incorrectly succeeded");
    next();
}

// First one should fail with preflight failure. Second one should
// fail with access control header failure.
var tests = [
    function() {
        var xhr = new XMLHttpRequest();
        xhr.onerror = success;
        xhr.onload = failure;
        xhr.open("GET", "http://127.0.0.1:8000/security/resources/cors-script.php?cors=false");
        xhr.setRequestHeader("x-custom-header", "foobar");
        xhr.send();
    },
    function() {
        var xhr = new XMLHttpRequest();
        xhr.onerror = success;
        xhr.onload = failure;
        xhr.open("GET", "http://127.0.0.1:8000/security/resources/cors-script.php?cors=false");
        xhr.send();
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
