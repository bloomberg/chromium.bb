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
<div id="container"></div>
<script>
var server = "http://127.0.0.1:8000";

var xorigin_anon_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';
var xorigin_creds_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?credentials=true&value=ran';
var xorigin_ineligible_script_string = 'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';

var xorigin_anon_style_string = 'http://127.0.0.1:8000/security/resources/cors-style.php?';
var xorigin_creds_style_string = 'http://127.0.0.1:8000/security/resources/cors-style.php?credentials=true';
var xorigin_ineligible_style_string = 'http://127.0.0.1:8000/security/resources/cors-style.php?';

function genCorsSrc(str, acao_value) {
    if (acao_value) {
        str = str + "&cors=" + acao_value;
    }
    return str;
}

function xoriginAnonScript(acao_value) { return genCorsSrc(xorigin_anon_script_string, acao_value); }
function xoriginCredsScript(acao_value) { return genCorsSrc(xorigin_creds_script_string, acao_value); }
function xoriginIneligibleScript() { return genCorsSrc(xorigin_ineligible_script_string, "false"); }


function xoriginAnonStyle(acao_value) { return genCorsSrc(xorigin_anon_style_string, acao_value); }
function xoriginCredsStyle(acao_value) { return genCorsSrc(xorigin_creds_style_string, acao_value); }
function xoriginIneligibleStyle() { return genCorsSrc(xorigin_ineligible_style_string, "false"); }

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

// <link> tests
// Style tests must be done synchronously because they rely on the presence and
// absence of global style, which can affect later tests. Thus, instead of
// executing them one at a time, the style tests are implemented as a queue
// that builds up a list of tests, and then executes them one at a time.
var SuboriginStyleTest = function(queue, pass, name, href, crossoriginValue) {
    this.queue = queue;
    this.pass = pass;
    this.name = "Style: " + name;
    this.href = href;
    this.crossoriginValue = crossoriginValue;
    this.test = async_test(this.name);

    this.queue.push(this);
}

SuboriginStyleTest.prototype.execute = function() {
    var container = document.getElementById("container");
    while(container.hasChildNodes()) {
        container.removeChild(container.firstChild);
    }

    var test = this.test;

    var div = document.createElement("div");
    div.className = "id1";
    var e = document.createElement("link");
    e.rel = "stylesheet";
    e.href = this.href;
    if(this.crossoriginValue) {
        e.setAttribute("crossorigin", this.crossoriginValue);
    }
    if(this.pass) {
        e.addEventListener("load", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_equals(background, "rgb(255, 255, 0)");
                test.done();
            });
        });
        e.addEventListener("error", function() {
            test.step(function(){ assert_unreached("Good load fired error handler."); });
        });
    } else {
        e.addEventListener("load", function() {
             test.step(function() { assert_unreached("Bad load succeeded."); });
         });
        e.addEventListener("error", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_equals(background, "rgba(0, 0, 0, 0)");
                test.done();
            });
        });
    }
    container.appendChild(div);
    container.appendChild(e);
}

var style_tests = [];
style_tests.execute = function() {
    if (this.length > 0) {
        this.shift().execute();
    }
}
add_result_callback(function(res) {
    if (res.name.startsWith("Style: ")) {
        style_tests.execute();
    }
});

// XMLHttpRequest tests
var SuboriginXHRTest = function(pass, name, src, crossoriginValue) {
    this.pass = pass;
    this.name = "XHR: " + name;
    this.src = src;
    this.crossoriginValue = crossoriginValue;
}

SuboriginXHRTest.prototype.execute = function() {
    var test = async_test(this.name);
    var xhr = new XMLHttpRequest();

    if (this.crossoriginValue === 'use-credentials') {
        xhr.withCredentials = true;
    }

    if (this.pass) {
        xhr.onload = function() {
            test.done();
        };
        xhr.onerror = function() {
            test.step(function() { assert_unreached("Good XHR fired error handler."); });
        };
    } else {
        xhr.onload = function() {
            test.step(function() { assert_unreached("Bad XHR successful."); });
        };
        xhr.onerror = function() {
            test.done();
        };
    }

    xhr.open("GET", this.src);
    xhr.send();
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

// Style tests
new SuboriginStyleTest(
    style_tests,
    false,
    "<crossorigin='anonymous'>, ACAO: " + server,
    xoriginAnonStyle(),
    "anonymous");

new SuboriginStyleTest(
    style_tests,
    true,
    "<crossorigin='anonymous'>, ACAO: *",
    xoriginAnonStyle('*'),
    "anonymous").execute();

new SuboriginStyleTest(
    style_tests,
    false,
    "<crossorigin='use-credentials'>, ACAO: " + server,
    xoriginCredsStyle(),
    "use-credentials").execute();

new SuboriginStyleTest(
    style_tests,
    false,
    "<crossorigin='anonymous'>, CORS-ineligible resource",
    xoriginIneligibleStyle(),
    "anonymous").execute();

style_tests.execute();

// XHR tests
new SuboriginXHRTest(
    false,
    "<crossorigin='anonymous'>, ACAO: " + server,
    xoriginAnonScript(),
    "anonymous").execute();

new SuboriginXHRTest(
    true,
    "<crossorigin='anonymous'>, ACAO: *",
    xoriginAnonScript('*'),
    "anonymous").execute();

new SuboriginXHRTest(
    false,
    "<crossorigin='use-credentials'>, ACAO: " + server,
    xoriginCredsScript(),
    "use-credentials").execute();

new SuboriginXHRTest(
    false,
    "<crossorigin='anonymous'>, CORS-ineligible resource",
    xoriginIneligibleScript(),
    "anonymous").execute();

</script>
</body>
</html>
