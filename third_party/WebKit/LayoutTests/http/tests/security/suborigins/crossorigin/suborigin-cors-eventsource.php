<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Verify CORS behavior on Suborigin using EventSource</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="/security/suborigins/resources/suborigin-cors-lib.js"></script>
</head>
<body>
<div id="container"></div>
<script>
var xorigin_anon_event_source_string = 'http://127.0.0.1:8000/eventsource/resources/es-cors-basic.php?';
var xorigin_creds_event_source_string = 'http://127.0.0.1:8000/eventsource/resources/es-cors-credentials.php?';
var xorigin_ineligible_event_source_string = 'http://127.0.0.1:8000/eventsource/resources/es-cors-basic.php?';

function xoriginAnonEventSource(acao_value) { return genCorsSrc(xorigin_anon_event_source_string, acao_value); }
function xoriginCredsEventSource(acao_value) { return genCorsSrc(xorigin_creds_event_source_string, acao_value); }
function xoriginIneligibleEventSource(acao_value) { return genCorsSrc(xorigin_ineligible_event_source_string); }

// EventSource tests
var SuboriginEventSourceTest = function(pass, name, src, crossoriginValue) {
    SuboriginTest.call(this, pass, "EventSource: " + name, src, crossoriginValue);
}

SuboriginEventSourceTest.prototype.execute = function() {
    var test = async_test(this.name);
    var pass = this.pass;
    var options = {};

    if (this.crossoriginValue === 'use-credentials') {
        options.withCredentials = 'include';
    }

    var es = new EventSource(this.src, options);
    es.onmessage = test.step_func(function() {
        if (pass) {
            test.done();
        } else {
            assert_unreached("Bad EventSource received message.");
        }
    });
    es.onerror = test.step_func(function() {
        if (pass) {
            assert_unreached("Good EventSource failed to connect.");
        } else {
            test.done();
        }
    });
};

// EventSource tests
new SuboriginEventSourceTest(
    false,
    "anonymous, ACAO: " + server,
    xoriginAnonEventSource(),
    "anonymous").execute();

new SuboriginEventSourceTest(
    true,
    "anonymous, ACAO: *",
    xoriginAnonEventSource('*'),
    "anonymous").execute();

new SuboriginEventSourceTest(
    false,
    "use-credentials, ACAO: " + server,
    xoriginCredsEventSource(),
    "use-credentials").execute();

new SuboriginEventSourceTest(
    false,
    "anonymous, CORS-ineligible resource",
    xoriginIneligibleEventSource(),
    "anonymous").execute();
</script>
</body>
</html>
