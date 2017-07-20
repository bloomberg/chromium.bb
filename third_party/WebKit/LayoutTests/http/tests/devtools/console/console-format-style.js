<html>
    <head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function onload()
{
    console.log("%cBlue!.", "color: blue;");
    console.log("%cBlue! %cRed!", "color: blue;", "color: red;");
    console.log("%cwww.google.com", "color: blue");
    runTest();
}

function test()
{
    InspectorTest.expandConsoleMessages(onExpanded);

    function onExpanded()
    {
        InspectorTest.dumpConsoleMessagesWithStyles();
        InspectorTest.completeTest();
    }
}
</script>
    </head>

    <body onload="onload()">
      <p>Tests that console logging dumps properly styled messages.</p>
    </body>
</html>
