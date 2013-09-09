var initialize_ConsoleTest = function() {

InspectorTest.showConsolePanel = function()
{
    WebInspector.showPanel("console");
    WebInspector.drawer.immediatelyFinishAnimation();
}

InspectorTest.prepareConsoleMessageText = function(messageElement)
{
    var messageText = messageElement.textContent.replace(/\u200b/g, "");
    // Replace scriptIds with generic scriptId string to avoid flakiness.
    messageText = messageText.replace(/VM\d+/g, "VM");
    // Strip out InjectedScript from stack traces to avoid rebaselining each time InjectedScriptSource is edited.
    messageText = messageText.replace(/InjectedScript[\.a-zA-Z_]+ VM:\d+/g, "");
    // The message might be extremely long in case of dumping stack overflow message.
    messageText = messageText.substring(0, 1024);
    return messageText;
}

InspectorTest.dumpConsoleMessages = function(printOriginatingCommand, dumpClassNames)
{
    var result = [];
    var visibleMessagesIndices = WebInspector.consoleView._visibleMessagesIndices;
    for (var i = 0; i < visibleMessagesIndices.length; ++i) {
        var message = WebInspector.console.messages[visibleMessagesIndices[i]];
        var element = message.toMessageElement();

        if (dumpClassNames) {
            var classNames = [];
            for (var node = element.firstChild; node; node = node.traverseNextNode(element)) {
                if (node.nodeType === Node.ELEMENT_NODE && node.className)
                    classNames.push(node.className);
            }
        }

        var messageText = InspectorTest.prepareConsoleMessageText(element)
        InspectorTest.addResult(messageText + (dumpClassNames ? " " + classNames.join(" > ") : ""));
        if (printOriginatingCommand && message.originatingCommand) {
            var originatingElement = message.originatingCommand.toMessageElement();
            InspectorTest.addResult("Originating from: " + originatingElement.textContent.replace(/\u200b/g, ""));
        }
    }
    return result;
}

InspectorTest.dumpConsoleMessagesWithStyles = function(sortMessages)
{
    var result = [];
    var indices = WebInspector.consoleView._visibleMessagesIndices;
    for (var i = 0; i < indices.length; ++i) {
        var element = WebInspector.console.messages[indices[i]].toMessageElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        InspectorTest.addResult(messageText);
        var spans = element.querySelectorAll(".console-message-text > span > span");
        for (var j = 0; j < spans.length; j++)
            InspectorTest.addResult("Styled text #" + j + ": " + (spans[j].style.cssText || "NO STYLES DEFINED"));
    }
}

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    var result = [];
    var indices = WebInspector.consoleView._visibleMessagesIndices;
    for (var i = 0; i < indices.length; ++i) {
        var element = WebInspector.console.messages[indices[i]].toMessageElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        result.push(messageText + " " + element.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < result.length; ++i)
        InspectorTest.addResult(result[i]);
}

InspectorTest.expandConsoleMessages = function()
{
    var indices = WebInspector.consoleView._visibleMessagesIndices;
    for (var i = 0; i < indices.length; ++i) {
        var element = WebInspector.console.messages[indices[i]].toMessageElement();
        var node = element;
        while (node) {
            if (node.treeElementForTest)
                node.treeElementForTest.expand();
            node = node.traverseNextNode(element);
        }
    }
}

InspectorTest.checkConsoleMessagesDontHaveParameters = function()
{
    var indices = WebInspector.consoleView._visibleMessagesIndices;
    for (var i = 0; i < indices.length; ++i) {
        var m = WebInspector.console.messages[indices[i]];
        InspectorTest.addResult("Message[" + i + "]:");
        InspectorTest.addResult("Message: " + WebInspector.displayNameForURL(m.url) + ":" + m.line + " " + m.message);
        if ("_parameters" in m) {
            if (m._parameters)
                InspectorTest.addResult("FAILED: message parameters list is not empty: " + m._parameters);
            else
                InspectorTest.addResult("SUCCESS: message parameters list is empty. ");
        } else {
            InspectorTest.addResult("FAILED: didn't find _parameters field in the message.");
        }
    }
}

InspectorTest.waitUntilMessageReceived = function(callback)
{
    InspectorTest.addSniffer(WebInspector.console, "addMessage", callback, false);
}

}
