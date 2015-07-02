<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Allow suborigin in HTTP header</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var server = "http://127.0.0.1:8000";
var xorigin_anon_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';
var xorigin_creds_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?credentials=true&value=ran';
var xorigin_ineligible_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';

function genCorsSrc(str, acao_value) {
    if (acao_value) {
        str = str + "&cors=" + acao_value;
    }
    return str;
}

function xoriginAnonScript(acao_value) { return genCorsSrc(xorigin_anon_script_string, acao_value); }
function xoriginCredsScript(acao_value) { return genCorsSrc(xorigin_creds_script_string, acao_value); }
function xoriginIneligibleScript() { return genCorsSrc(xorigin_ineligible_script_string, "false"); }


var SuboriginScriptTest = function(pass, name, src, crossoriginValue) {
    this.pass = pass;
    this.name = "Script: " + name;
    this.src = src;
    this.crossoriginValue = crossoriginValue;
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
