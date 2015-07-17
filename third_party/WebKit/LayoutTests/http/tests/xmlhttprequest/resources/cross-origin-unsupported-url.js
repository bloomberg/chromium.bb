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

    if (self.importScripts) {
        // Initiating the load on the main thread is not performed synchronously,
        // so send() will not have an exception code set by the time it
        // completes in the Worker case. Hence, no exception will be
        // thrown by the operation.
        shouldNotThrow('xhr.send()');
    } else {
        // The implementation of send() throws an exception if an
        // exception code has been set, regardless of the sync flag.
        // The spec restricts this to sync only, but as error progress
        // events provide no actionable information, it is more helpful
        // to the user to not follow spec.
        //
        // As the initiation of the request happens synchronously in send(),
        // and it is determined that it is to an unsupported CORS URL, an
        // exception is expected to be thrown.
        shouldThrow('xhr.send()');
    }
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
