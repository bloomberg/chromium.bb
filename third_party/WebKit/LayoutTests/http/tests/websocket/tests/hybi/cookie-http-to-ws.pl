#!/usr/bin/perl -wT
use strict;

if ($ENV{"QUERY_STRING"} eq "clear=1") {
    print "Content-Type: text/plain\r\n";
    print "Set-Cookie: WK-websocket-test-path=0; Path=/websocket/tests/hybi/cookie-http-to-ws.pl; Max-Age=0\r\n";
    print "Set-Cookie: WK-websocket-test-domain=0; Domain=127.0.0.1; Max-Age=0\r\n";
    print "\r\n";
    print "Cookies are cleared.";
    exit;
}

print "Content-Type: text/html\r\n";
print "Set-Cookie: WK-websocket-test-path=1; Path=/websocket/tests/hybi/cookie-http-to-ws.pl\r\n";
print "Set-Cookie: WK-websocket-test-domain=1; Domain=127.0.0.1\r\n";
print "\r\n";
print <<HTML
<html>
<head>
<script src="/js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test How WebSocket handles cookies with cookie-av's.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

function checkCookie(expected, actual) {
    expected = expected.split('; ').sort().join('; ');
    actual = actual.split('; ').sort().join('; ');
    if (expected === actual) {
        debug('PASS cookie is "' + expected + '"');
    } else {
        debug('FAIL cookie should be ' + expected + '. Was ' + actual + '.');
    }
}

checkCookie('WK-websocket-test-path=1; WK-websocket-test-domain=1', document.cookie);

var cases = [{url: "ws://127.0.0.1:8880/websocket/tests/hybi/echo-cookie",
              cookie: "WK-websocket-test-domain=1"}];

function run(testcase, callback, failCallback) {
    var ws = new WebSocket(testcase.url);
    var cookie;
    ws.onopen = function () {
        debug('A WebSocket connection to ' + testcase.url + ' is opened.');
    };
    ws.onmessage = function (e) {
        cookie = e.data;
        ws.close();
    };
    ws.onclose = function (e) {
        debug('A WebSocket connection to ' + testcase.url + ' is closed.');
        callback(cookie);
    };
    ws.onerror = failCallback;
}

function clear() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'cookie-http-to-ws.pl?clear=1', false);
    xhr.send(null);
}

var runs = [];
for (var i = cases.length - 1; i >= 0; i -= 1) {
    (function (i) {
        var next = (i === cases.length - 1 ? function () { clear(); finishJSTest(); } : runs[i + 1]);
        runs[i] = run.bind(null,
                           cases[i],
                           function (cookie) {
                               checkCookie(cases[i].cookie, cookie);
                               next();
                           },
                           function () {
                               debug('Test #' + i + ' failed.');
                               next();
                           });
    })(i);
}
runs[0]();
</script>
</body>
</html>
HTML
