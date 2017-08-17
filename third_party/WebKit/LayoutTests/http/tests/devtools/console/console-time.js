<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

testRunner.setDumpConsoleMessages(false);

function testFunction()
{
    console.time();
    console.timeEnd();
    console.time("42");
    console.timeEnd("42");
    console.time(239)
    console.timeEnd(239);
    console.time({});
    console.timeEnd({});
}

function test()
{
    InspectorTest.waitUntilNthMessageReceived(4, dumpMessagesAndCompleTest);
    InspectorTest.evaluateInPage("testFunction()");

    function dumpMessagesAndCompleTest()
    {
        var messages = InspectorTest.dumpConsoleMessagesIntoArray();
        messages = messages.map(message => message.replace(/\d+\.\d+ms/, "<time>"));
        InspectorTest.addResults(messages);
        InspectorTest.completeTest();
    }
}
</script>
</head>
<body onload="runTest()">
<p>
console.time / console.timeEnd tests.
</p>
</body>
</html>
