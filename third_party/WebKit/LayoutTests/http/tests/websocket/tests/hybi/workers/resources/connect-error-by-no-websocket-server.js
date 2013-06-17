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

var url = "ws://127.0.0.1:8888"; // This port must be closed.

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

// On Windows, a localhost "Connection Refused" takes around 1 second. Since we
// need to have 3 connections refused in serial, we need > 3 seconds on
// Windows.
var timeoutID = setTimeout(timeOutCallback, 4000);

doTest(0);
