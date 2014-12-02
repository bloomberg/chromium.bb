<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function logToConsole()
{
    var fragment = document.createDocumentFragment();
    fragment.appendChild(document.createElement("p"));

    console.dirxml(document);
    console.dirxml(fragment);
    console.dirxml(fragment.firstChild);
    console.log([fragment.firstChild]);
    console.dirxml([document, fragment, document.createElement("span")]);
}

function test()
{
    runtime.loadModulePromise("elements").then(function() {
        InspectorTest.evaluateInPage("logToConsole()", onLoggedToConsole);
    });

    function onLoggedToConsole()
    {
        InspectorTest.waitForRemoteObjectsConsoleMessages(onRemoteObjectsLoaded)
    }

    function onRemoteObjectsLoaded()
    {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="runTest()">
<p>
Tests that console logging dumps proper messages.
</p>

</body>
</html>
