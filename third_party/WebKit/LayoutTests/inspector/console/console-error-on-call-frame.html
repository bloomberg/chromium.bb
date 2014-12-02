<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/debugger-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function testFunction()
{
    var i = 0;
    debugger;
}

function test()
{
    InspectorTest.startDebuggerTest(step1);

    function step1()
    {
        InspectorTest.runTestFunctionAndWaitUntilPaused(step2);
    }

    function step2()
    {
        InspectorTest.evaluateInConsole("console.error(42)", step3);
    }

    function step3()
    {
        InspectorTest.resumeExecution(step4);
    }

    function step4()
    {
        InspectorTest.expandConsoleMessages(onExpanded);
    }

    function onExpanded()
    {
        var result = InspectorTest.dumpConsoleMessagesIntoArray().join("\n");
        result = result.replace(/(\(program\)):\d+/g, "$1");
        InspectorTest.addResult(result);
        InspectorTest.completeDebuggerTest();
    }
}
</script>
</head>
<body onload="runTest()">
<p>Tests that console.error does not throw exception when executed in console on call frame.</p>
</body>
</html>
