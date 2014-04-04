if (self.postMessage)
    runTests();
else
    onconnect = handleConnect;

function handleConnect(event)
{
    self.postMessage = function (message) {
        event.ports[0].postMessage(message);
    };
    runTests();
}

function setCookie() {
    return new Promise(function (resolve, reject)
    {
        var opened = false;

        var ws = new WebSocket('ws://127.0.0.1:8880/set-cookie');
        ws.onopen = function () {
            opened = true;
            ws.close();
        };
        ws.onmessage = function (evt) {
            reject('Unexpected message event on set-cookie socket');
        };
        ws.onerror = function () {
            reject('Error on set-cookie socket');
        };
        ws.onclose = function (evt) {
            if (opened)
                resolve();
            else
                reject('Close event handler is called on set-cookie socket before receiving open event');
        };
    });
}

function echoCookie() {
    return new Promise(function (resolve, reject)
    {
        var ws = new WebSocket('ws://127.0.0.1:8880/echo-cookie');
        ws.onmessage = function (evt) {
            resolve(evt.data);
        };
        ws.onerror = function () {
            reject('Error on echo-cookie socket');
        };
        ws.onclose = function (evt) {
            reject('Close event handler is called on echo-cookie socket before receiving any message event');
        };
    });
}

function runTests()
{
    setCookie().then(
        function () {
            return echoCookie();
        }
    ).then(
        function (cookie) {
            if (cookie != 'WK-WebSocket-test-domain-pass=1; WK-WebSocket-test-path-pass=1; WK-WebSocket-test=1')
                return Promise.reject('Echoed cookie is incorrect');

            postMessage("DONE");
        }
    ).catch(
        function (reason) {
            postMessage('FAIL: ' + reason);
        }
    );
}
