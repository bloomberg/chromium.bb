if (self.importScripts)
    importScripts("/js-test-resources/js-test.js");

self.jsTestIsAsync = true;

description("Test cross-origin XHRs to CORS-unsupported protocol schemes in the URL.");

var xhr;
var errorEvent;
function issueRequest(url, contentType)
{
    xhr = new XMLHttpRequest();
    xhr.open('POST', url);
    xhr.onerror = function (a) {
        errorEvent = a;
        shouldBeEqualToString("errorEvent.type", "error");
        setTimeout(runTest, 0);
    };
    // Assumed a Content-Type that turns it into a non-simple CORS request.
    if (contentType)
        xhr.setRequestHeader('Content-Type', contentType);

    shouldNotThrow('xhr.send()');
}

var withContentType = true;
var tests = [ 'http://localhost:1291a/',
              'ftp://127.0.0.1',
              'localhost:8080/',
              'tel:1234' ];

function runTest()
{
    if (!tests.length && withContentType) {
        finishJSTest();
        return;
    }
    withContentType = !withContentType;
    if (!withContentType)
        issueRequest(tests[0]);
    else
        issueRequest(tests.shift(), 'application/json');
}
runTest();
