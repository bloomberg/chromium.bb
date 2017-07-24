<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function onload()
{
    var a = {};
    for (var i = 0; i < 100; ++i)
        a[i] = i;
    console.dir(a);

    runTest();
}

function test()
{
    InspectorTest.expandConsoleMessages(onConsoleMessageExpanded);

    function onConsoleMessageExpanded()
    {
        var messages = Console.ConsoleView.instance()._visibleViewMessages;
        for (var i = 0; i < messages.length; ++i) {
            var message = messages[i];
            var node = message.contentElement();
            for (var node = message.contentElement(); node; node = node.traverseNextNode(message.contentElement())) {
                if (node.treeElement) {
                    onTreeElement(node.treeElement.firstChild());
                    return;
                }
            }
        }
    }

    function onTreeElement(treeElement)
    {
        treeElement._startEditing();
        Console.ConsoleView.instance()._viewport.refresh();
        InspectorTest.addResult("After viewport refresh tree element remains in editing mode: " + !!treeElement._prompt);
        InspectorTest.completeTest();
    }
}

</script>
</head>

<body onload="onload()">
<p>
Tests that expanded tree element is editable in console.
</p>

</body>
</html>
