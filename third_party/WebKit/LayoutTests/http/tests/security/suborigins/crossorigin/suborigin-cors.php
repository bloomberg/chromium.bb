<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Allow suborigin in HTTP header</title>
<meta name="timeout" content="long">
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

var xorigin_anon_hello_string = 'http://127.0.0.1:8000/security/resources/cors-hello.php?';
var xorigin_creds_hello_string = 'http://127.0.0.1:8000/security/resources/cors-hello.php?credentials=true';
var xorigin_ineligible_hello_string = 'http://127.0.0.1:8000/security/resources/cors-hello.php?';

var xorigin_font_string = 'http://127.0.0.1:8000/security/resources/cors-font.php?';

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

function xoriginAnonHello(acao_value) { return genCorsSrc(xorigin_anon_hello_string, acao_value); }
function xoriginCredsHello(acao_value) { return genCorsSrc(xorigin_creds_hello_string, acao_value); }
function xoriginIneligibleHello() { return genCorsSrc(xorigin_ineligible_hello_string, "false"); }

function xoriginFont(acao_value) { return genCorsSrc(xorigin_font_string, acao_value); }
function xoriginIneligibleFont() { return genCorsSrc(xorigin_font_string, "false"); }

var SuboriginTest = function(pass, name, src, crossoriginValue) {
    this.pass = pass;
    this.name = name;
    this.src = src;
    this.crossoriginValue = crossoriginValue;
}
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

// <link> tests
// Link tests must be done synchronously because they rely on the presence and
// absence of global style, which can affect later tests. Thus, instead of
// executing them one at a time, the link tests are implemented as a queue
// that builds up a list of tests, and then executes them one at a time.
var SuboriginLinkTest = function(queue, rel, pass_verify, fail_verify, pass, name, href, crossoriginValue) {
    this.queue = queue;
    this.rel = rel;
    this.pass_verify = pass_verify;
    this.fail_verify = fail_verify;

    this.queue.push(this);

    SuboriginTest.call(this, pass, name, href, crossoriginValue);
}

SuboriginLinkTest.prototype.execute = function() {
    var container = document.getElementById("container");
    while (container.hasChildNodes()) {
        container.removeChild(container.firstChild);
    }

    var test = async_test(this.name);
    var pass_verify = this.pass_verify;
    var fail_verify = this.fail_verify;

    var div = document.createElement("div");
    div.className = "id1";
    var e = document.createElement("link");
    e.rel = this.rel;
    e.href = this.src;
    if (this.crossoriginValue) {
        e.setAttribute("crossorigin", this.crossoriginValue);
    }
    if (this.pass) {
        e.addEventListener("load", function() {
            test.step(function() {
                pass_verify(div, e);
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
                fail_verify(div, e);
                test.done();
            });
        });
    }
    container.appendChild(div);
    container.appendChild(e);
}

var link_tests = [];
link_tests.next = function() {
    if (link_tests.length > 0) {
        link_tests.shift().execute();
    }
}

var SuboriginStyleTest = function(pass, name, href, crossoriginValue) {
    var style_pass_verify = function (div, e) {
        var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
        assert_equals(background, "rgb(255, 255, 0)");
    };

    var style_fail_verify = function(div, e) {
        var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
        assert_equals(background, "rgba(0, 0, 0, 0)");
    };

    SuboriginLinkTest.call(this, link_tests, "stylesheet", style_pass_verify, style_fail_verify, pass, "Style: " + name, href, crossoriginValue);
};
SuboriginStyleTest.prototype = Object.create(SuboriginLinkTest.prototype);

var SuboriginHTMLImportTest = function(pass, name, href, crossoriginValue) {
    var import_fail_verify = function(div, e) {
        assert_equals(e.import, null);
    };
    SuboriginLinkTest.call(this, link_tests, "import", function(){}, import_fail_verify, pass, "Import: " + name, href, crossoriginValue);
};
SuboriginHTMLImportTest.prototype = Object.create(SuboriginLinkTest.prototype);

add_result_callback(function(res) {
    if (res.name.startsWith("Style: ") || res.name.startsWith("Import: ")) {
        link_tests.next();
    }
});

// XMLHttpRequest tests
var SuboriginXHRTest = function(pass, name, src, crossoriginValue) {
    SuboriginTest.call(this, pass, "XHR: " + name, src, crossoriginValue);
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
};

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

// FontFace tests
var SuboriginFontFaceTest = function(pass, name, src) {
    SuboriginTest.call(this, pass, "FontFace: " + name, src, false);
};

SuboriginFontFaceTest.prototype.execute = function() {
    var test = async_test(this.name);
    var url = "url('" + this.src + "')";
    var fontFace = new FontFace("ahem", url);

    if (this.pass) {
        fontFace.load().then(
            function(loadedFace) {
                test.step(function() {
                    assert_equals(loadedFace.family, "ahem");
                    test.done();
                })},
            function() {
                test.step(function() {
                    assert_unreached("Good FontFace failed to load.");
                    test.done();
                });
            });
    } else {
        fontFace.load().then(
            function() {
                test.step(function() {
                    assert_unreached("Bad FontFace loaded.");
                    test.done();
                })},
            function() {
                test.step(function() {
                    test.done();
                });
            });
    }
};

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
    false,
    "<crossorigin='anonymous'>, ACAO: " + server,
    xoriginAnonStyle(),
    "anonymous");

new SuboriginStyleTest(
    true,
    "<crossorigin='anonymous'>, ACAO: *",
    xoriginAnonStyle('*'),
    "anonymous");

new SuboriginStyleTest(
    false,
    "<crossorigin='use-credentials'>, ACAO: " + server,
    xoriginCredsStyle(),
    "use-credentials");

new SuboriginStyleTest(
    false,
    "<crossorigin='anonymous'>, CORS-ineligible resource",
    xoriginIneligibleStyle(),
    "anonymous");

// HTML Imports tests
new SuboriginHTMLImportTest(
    false,
    "<crossorigin='anonymous'>, ACAO: " + server,
    xoriginAnonHello(),
    "anonymous");

new SuboriginHTMLImportTest(
    true,
    "<crossorigin='anonymous'>, ACAO: *",
    xoriginAnonHello('*'),
    "anonymous");

new SuboriginHTMLImportTest(
    false,
    "<crossorigin='use-credentials'>, ACAO: " + server,
    xoriginCredsHello(),
    "use-credentials");

new SuboriginHTMLImportTest(
    false,
    "<crossorigin='anonymous'>, CORS-ineligible resource",
    xoriginIneligibleHello(),
    "anonymous");

link_tests.next();

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

// CSS FontFace tests
new SuboriginFontFaceTest(
    false,
    "ACAO: " + server,
    xoriginFont()).execute();
new SuboriginFontFaceTest(
    true,
    "ACAO: *",
    xoriginFont('*')).execute();
new SuboriginFontFaceTest(
    false,
    "CORS-ineligible resource",
    xoriginIneligibleFont()).execute();
</script>
</body>
</html>
