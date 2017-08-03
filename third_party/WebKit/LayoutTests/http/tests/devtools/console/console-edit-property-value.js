<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function logToConsole()
{
    var obj = {a: 1, b: "foo", c: null, d: 2};
    console.log(obj);
}

var test = function()
{
    InspectorTest.evaluateInConsole("logToConsole()", step1);

    function step1()
    {
        InspectorTest.expandConsoleMessages(step2);
    }

    function step2()
    {
        var valueElements = getValueElements();
        doubleClickTypeAndEnter(valueElements[0], "1 + 2");
        InspectorTest.waitForRemoteObjectsConsoleMessages(step3);
    }

    function step3()
    {
        var valueElements = getValueElements();
        doubleClickTypeAndEnter(valueElements[1], "nonExistingValue");
        InspectorTest.waitForRemoteObjectsConsoleMessages(step4);
    }

    function step4()
    {
        var valueElements = getValueElements();
        doubleClickTypeAndEnter(valueElements[2], "[1, 2, 3]");
        InspectorTest.waitForRemoteObjectsConsoleMessages(step5);
    }

    function step5()
    {
        var valueElements = getValueElements();
        doubleClickTypeAndEnter(valueElements[3], "{x: 2}");
        InspectorTest.waitForRemoteObjectsConsoleMessages(step6);
    }

    function step6()
    {
        InspectorTest.dumpConsoleMessagesIgnoreErrorStackFrames();
        InspectorTest.completeTest();
    }

    function getValueElements()
    {
        var messageElement = Console.ConsoleView.instance()._visibleViewMessages[1].element();
        return messageElement.querySelector(".console-message-text *").shadowRoot.querySelectorAll(".value");
    }

    function doubleClickTypeAndEnter(node, text)
    {
        var event = document.createEvent("MouseEvent");
        event.initMouseEvent("dblclick", true, true, null, 2);
        node.dispatchEvent(event);

        InspectorTest.addResult("Node was hidden after dblclick: " + node.classList.contains("hidden"));

        var messageElement = Console.ConsoleView.instance()._visibleViewMessages[1].element();
        var editPrompt = messageElement.querySelector(".console-message-text *").shadowRoot.querySelector(".text-prompt");
        editPrompt.textContent = text;
        editPrompt.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
    }
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that property values can be edited inline in the console via double click.
</p>

</body>
</html>
