<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
async function test()
{
    await InspectorTest.DebuggerAgent.setAsyncCallStackDepth(200);

    InspectorTest.waitUntilNthMessageReceived(1, step2);
    InspectorTest.evaluateInPage("setTimeout(\"~\", 0)");

    function step2()
    {
        InspectorTest.expandConsoleMessages(step3);
    }

    function step3()
    {
        InspectorTest.dumpConsoleMessagesIgnoreErrorStackFrames();
        InspectorTest.completeTest();
    }
};
</script>
</head>
<body onload="runTest()">
<p>Tests exception message with empty stack in console contains async stack trace.</p>
</body>
</html>
