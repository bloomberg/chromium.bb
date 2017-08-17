<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
// Both the call and the function entry may trigger stack overflow.
// Intentionally keep both on the same line to avoid flaky test failures.
function overflow() { overflow(); }

function doOverflow()
{
    setTimeout(overflow, 0);
}

function test()
{
    InspectorTest.evaluateInPage("doOverflow()", step2.bind(this));

    function step2()
    {
        if (Console.ConsoleView.instance()._visibleViewMessages.length < 1)
            InspectorTest.addConsoleSniffer(step2);
        else
            step3();
    }

    function step3()
    {
        InspectorTest.expandConsoleMessages(onExpanded);
    }

    function onExpanded()
    {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    }
}
</script>
</head>
<body onload="runTest()">
<p>Tests that when stack overflow exception happens when inspector is open the stack trace is correctly shown in console.</p>
</body>
</html>
