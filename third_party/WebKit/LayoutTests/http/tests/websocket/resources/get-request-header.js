var ECHO_REQUEST_HEADERS_WS_URL = 'ws://127.0.0.1:8880/echo-request-headers';

// Returns a Promise which will create a new WebSocket, and resolve to the value
// of the specified header in the request, or reject with an error message if
// something goes wrong. The header must be specified in lower-case.
function connectAndGetRequestHeader(request_header)
{
    return connectAndGetRequestHeaders().then(function(headers)
    {
        return headers[request_header];
    });
}

// Returns a Promise which will create a new WebSocket, and return all the
// request headers as an object, with the header names as keys in lower-case.
function connectAndGetRequestHeaders()
{
    return new Promise(function(resolve, reject) {
        var header_ws = new WebSocket(ECHO_REQUEST_HEADERS_WS_URL);
        header_ws.onmessage = function (evt) {
            try {
                var headers = JSON.parse(evt.data);
                resolve(headers);
            } catch (e) {
                reject("Unexpected exception from JSON.parse: " + e);
            }
            // The stringify() call in header_ws.onclose can cause a
            // "'WebSocket.URL' is deprecated." error as a side-effect even
            // though the reject() call itself does nothing. Clear out the
            // handlers to prevent unwanted side-effects.
            header_ws.onerror = undefined;
            header_ws.onclose = undefined;
        };
        header_ws.onerror = function () {
            reject('Unexpected error event');
        };
        header_ws.onclose = function (evt) {
            reject('Unexpected close event: ' + JSON.stringify(evt));
        };
    });
}

// A useful function to use when setting the rejected handler for Promises
// returned by the above function. Reports the error and terminates the test.
// Use like
// connectAndGetRequestHeader('foo').then(function(value) {
//     expectation(value);
//     finishJSTest();
// }, finishAsFailed);
function finishAsFailed(message) {
    testFailed(message);
    finishJSTest();
}

// Extracts the value of a header from a WebSocket MessageEvent.
function getRequestHeaderFromEvent(message_event, request_header)
{
    return JSON.parse(message_event.data)[request_header];
}
