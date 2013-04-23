function postResult(result, actual, expected)
{
    var message = result ? "PASS" : "FAIL";
    message += ": worker: " + actual + " is ";
    if (!result)
        message += "not ";
    message += expected;
    postMessage(message);
}

function testPassed(message)
{
    postMessage("PASS: " + message);
}

function testFailed(message)
{
    postMessage("FAIL: " + message);
}

function debug(message)
{
    postMessage(message);
}

function endTest()
{
    clearTimeout(timeoutID);
    postMessage("DONE");
}

var url = "ws://localhost:8888"; // Here it should have no websocket server to listen.

function doTest(index)
{
    debug("test" + index + " Start");

    var ws = new WebSocket(url);

    ws.onopen = function()
    {
        testFailed("Connected");
        endTest();
    };

    ws.onmessage = function(messageEvent)
    {
        testFailed("Received Message");
        ws.close();
        endTest();
    };

    if (index == 0) {
        ws.onclose = function()
        {
            testPassed("onclose was called");
            doTest(index + 1);
        };

        ws.onerror = function()
        {
            testPassed("onerror was called");
        };
    } else if (index == 1) {
        ws.onclose = function()
        {
            testPassed("onclose was called");
            ws.close();
            doTest(index + 1);
        };
        ws.onerror = function()
        {
            testPassed("onerror was called");
        };
    } else {
        ws.onclose = function()
        {
            testPassed("onclose was called");
            endTest();
        };
        ws.onerror = function()
        {
            testPassed("onerror was called");
            ws.close();
        };
    }
}

function timeOutCallback()
{
    debug("Timed out...");
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

doTest(0);
