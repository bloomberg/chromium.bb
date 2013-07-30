#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Credentials: true\n";
print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";

print <<EOF
<html><body>
<script>
thisDocument = document.currentScript.ownerDocument;
function loadScriptFrom(url) {
    var element = thisDocument.createElement("script");
    element.setAttribute("crossorigin", "");
    element.setAttribute("src", url);
    thisDocument.head.appendChild(element);
    return element;
}

loadScriptFrom('http://127.0.0.1:8000/htmlimports/resources/external-script.js');
loadScriptFrom('http://127.0.0.1:8000/htmlimports/resources/cors-js.cgi');
loadScriptFrom('http://127.0.0.1:8000/htmlimports/resources/cors-js-for-localhost.cgi');
window.setTimeout(function() { loadScriptFrom('run-check.js'); }, 100);
</script>
</body></html>
EOF
