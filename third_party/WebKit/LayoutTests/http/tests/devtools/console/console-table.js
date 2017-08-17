<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function start()
{
    a = {};
    b = {};

    for (var i = 0; i < 15; i++)
        b["a" + i] = "a" + i;

    for (var i = 0; i < 15; i++) {
        a["b" + i] = "b" + i;
        b["b" + i] = "b" + i;
    }

    c = [a, b, a, b];
    d = [b, a, b, a];

    runTest();
}

function test()
{
    InspectorTest.disableConsoleViewport();
    InspectorTest.addConsoleViewSniffer(messageAdded, true);

    var count = 4;
    function messageAdded(message)
    {
        if (count === 2 || count === 3)
            InspectorTest.dumpConsoleTableMessage(message, true);

        if (--count === 0)
            InspectorTest.completeTest();
    }

    InspectorTest.evaluateInConsole("console.table(c); console.table(d)");
}
</script>
</head>
<body onload="start()">
<p>
    Tests that console.table is properly rendered on tables with more than 20 columns(maxColumnsToRender).
</p>
</body>
</html>
