<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function a()
{
    console.trace(1, 2, 3);
}

function test()
{
    function callback()
    {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateInPage("setTimeout(a, 0)");
    InspectorTest.addConsoleSniffer(callback);
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that console.trace dumps arguments alongside the stack trace.
</p>

</body>
</html>
