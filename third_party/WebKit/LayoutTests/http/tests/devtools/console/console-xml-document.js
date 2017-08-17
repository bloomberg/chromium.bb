<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>

<script>
function onload()
{
    console.dirxml((new DOMParser()).parseFromString("<MixedCase> Test </MixedCase>", "text/xml"));
    var danglingNode = document.implementation.createDocument("", "books");
    console.dirxml(danglingNode.createElement("Book"));
    runTest();
}
//# sourceURL=console-xml-document.html
</script>

<script>
function test()
{
    // Warm up elements renderer.
    runtime.loadModulePromise("elements").then(function() {
        InspectorTest.expandConsoleMessages(callback);
    });

    function callback()
    {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="onload()">
<p>
Tests that XML document contents are logged using the correct case in the console.
</p>

</body>
</html>
