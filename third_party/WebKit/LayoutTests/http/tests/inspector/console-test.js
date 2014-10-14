var initialize_ConsoleTest = function() {

InspectorTest.preloadPanel("console");

InspectorTest.evaluateInConsole = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    var consoleView = WebInspector.ConsolePanel._view();
    consoleView.visible = true;
    consoleView._prompt.text = code;
    var event = document.createEvent("KeyboardEvent");
    event.initKeyboardEvent("keydown", true, true, null, "Enter", "");
    consoleView._prompt.proxyElement.dispatchEvent(event);
    InspectorTest.addConsoleViewSniffer(function(commandResult) {
        callback(commandResult.toMessageElement().textContent);
    });
}

InspectorTest.addConsoleViewSniffer = function(override, opt_sticky)
{
    var sniffer = function (viewMessage) {
        override(viewMessage);
    };

    InspectorTest.addSniffer(WebInspector.ConsoleView.prototype, "_consoleMessageAddedForTest", sniffer, opt_sticky);
}

InspectorTest.evaluateInConsoleAndDump = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    function mycallback(text)
    {
        InspectorTest.addResult(code + " = " + text);
        callback(text);
    }
    InspectorTest.evaluateInConsole(code, mycallback);
}

InspectorTest.prepareConsoleMessageText = function(messageElement, consoleMessage)
{
    var messageText = messageElement.textContent.replace(/\u200b/g, "");
    // Replace scriptIds with generic scriptId string to avoid flakiness.
    messageText = messageText.replace(/VM\d+/g, "VM");
    // Strip out InjectedScript line numbers from stack traces to avoid rebaselining each time InjectedScriptSource is edited.
    messageText = messageText.replace(/VM:\d+ InjectedScript\./g, " InjectedScript.");
    // Strip out InjectedScript line numbers from console message anchor.
    var functionName = consoleMessage && consoleMessage.stackTrace && consoleMessage.stackTrace[0] && consoleMessage.stackTrace[0].functionName || "";
    if (functionName.indexOf("InjectedScript") !== -1)
        messageText = messageText.replace(/\bVM:\d+/, ""); // Only first replace.
    if (messageText.startsWith("Navigated to")) {
        var fileName = messageText.split(" ").pop().split("/").pop();
        messageText = "Navigated to " + fileName;
    }
    // The message might be extremely long in case of dumping stack overflow message.
    messageText = messageText.substring(0, 1024);
    return messageText;
}

InspectorTest.disableConsoleViewport = function()
{
    InspectorTest.fixConsoleViewportDimensions(600, 2000);
}

InspectorTest.fixConsoleViewportDimensions = function(width, height)
{
    var viewport = WebInspector.ConsolePanel._view()._viewport;
    viewport.element.style.width = width + "px";
    viewport.element.style.height = height + "px";
    viewport.element.style.position = "absolute";
    viewport.invalidate();
}

InspectorTest.dumpConsoleMessages = function(printOriginatingCommand, dumpClassNames, formatter)
{
    InspectorTest.addResults(InspectorTest.dumpConsoleMessagesIntoArray(printOriginatingCommand, dumpClassNames, formatter));
}

InspectorTest.dumpConsoleMessagesIntoArray = function(printOriginatingCommand, dumpClassNames, formatter)
{
    formatter = formatter || InspectorTest.prepareConsoleMessageText;
    var result = [];
    InspectorTest.disableConsoleViewport();
    var consoleView = WebInspector.ConsolePanel._view();
    if (consoleView._needsFullUpdate)
        consoleView._updateMessageList();
    var viewMessages = consoleView._visibleViewMessages;
    for (var i = 0; i < viewMessages.length; ++i) {
        var uiMessage = viewMessages[i];
        var message = uiMessage.consoleMessage();
        var element = uiMessage.contentElement();

        if (dumpClassNames) {
            var classNames = [];
            for (var node = element.firstChild; node; node = node.traverseNextNode(element)) {
                if (node.nodeType === Node.ELEMENT_NODE && node.className)
                    classNames.push(node.className);
            }
        }

        if (InspectorTest.dumpConsoleTableMessage(uiMessage, false, result)) {
            if (dumpClassNames)
                result.push(classNames.join(" > "));
        } else {
            var messageText = formatter(element, message);
            result.push(messageText + (dumpClassNames ? " " + classNames.join(" > ") : ""));
        }

        if (printOriginatingCommand && uiMessage.consoleMessage().originatingMessage())
            result.push("Originating from: " + uiMessage.consoleMessage().originatingMessage().messageText);
    }
    return result;
}

InspectorTest.dumpConsoleTableMessage = function(viewMessage, forceInvalidate, results)
{
    if (forceInvalidate)
        WebInspector.ConsolePanel._view()._viewport.invalidate();
    var table = viewMessage.contentElement();
    var headers = table.querySelectorAll("th div");
    if (!headers.length)
        return false;

    var headerLine = "";
    for (var i = 0; i < headers.length; i++)
        headerLine += headers[i].textContent + " | ";

    addResult("HEADER " + headerLine);

    var rows = table.querySelectorAll(".data-container tr");

    for (var i = 0; i < rows.length; i++) {
        var row = rows[i];
        var rowLine = "";
        var items = row.querySelectorAll("td > span");
        for (var j = 0; j < items.length; j++)
            rowLine += items[j].textContent + " | ";

        if (rowLine.trim())
            addResult("ROW " + rowLine);
    }

    function addResult(x)
    {
        if (results)
            results.push(x);
        else
            InspectorTest.addResult(x);
    }

    return true;
}

InspectorTest.dumpConsoleMessagesWithStyles = function(sortMessages)
{
    var result = [];
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].contentElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        InspectorTest.addResult(messageText);
        var spans = element.querySelectorAll(".console-message-text > span > span");
        for (var j = 0; j < spans.length; j++)
            InspectorTest.addResult("Styled text #" + j + ": " + (spans[j].style.cssText || "NO STYLES DEFINED"));
    }
}

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    var result = [];
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].contentElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element);
        result.push(messageText + " " + element.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < result.length; ++i)
        InspectorTest.addResult(result[i]);
}

InspectorTest.expandConsoleMessages = function(callback, deepFilter)
{
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;

    // Initiate round-trips to fetch necessary data for further rendering.
    for (var i = 0; i < messageViews.length; ++i)
        messageViews[i].contentElement();

    InspectorTest.runAfterPendingDispatches(expandTreeElements);

    function expandTreeElements()
    {
        for (var i = 0; i < messageViews.length; ++i) {
            var element = messageViews[i].contentElement();
            for (var node = element; node; node = node.traverseNextNode(element)) {
                if (node.treeElementForTest)
                    node.treeElementForTest.expand();
                if (!node._section)
                    continue;
                node._section.expanded = true;

                if (!deepFilter)
                    continue;
                var treeElements = node._section.propertiesTreeOutline.children;
                for (var j = 0; j < treeElements.length; ++j) {
                    for (var treeElement = treeElements[j]; treeElement; treeElement = treeElement.traverseNextTreeElement(false, null, false)) {
                        if (deepFilter(treeElement))
                            treeElement.expand();
                    }
                }
            }
        }
        InspectorTest.runAfterPendingDispatches(callback);
    }
}

InspectorTest.waitForRemoteObjectsConsoleMessages = function(callback)
{
    var messages = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messages.length; ++i)
        messages[i].toMessageElement();
    InspectorTest.runAfterPendingDispatches(callback);
}

InspectorTest.checkConsoleMessagesDontHaveParameters = function()
{
    var messageViews = WebInspector.ConsolePanel._view()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var m = messageViews[i].consoleMessage();
        InspectorTest.addResult("Message[" + i + "]:");
        InspectorTest.addResult("Message: " + WebInspector.displayNameForURL(m.url) + ":" + m.line + " " + m.message);
        if ("_parameters" in m) {
            if (m._parameters)
                InspectorTest.addResult("FAILED: message parameters list is not empty: " + m.parameters);
            else
                InspectorTest.addResult("SUCCESS: message parameters list is empty. ");
        } else {
            InspectorTest.addResult("FAILED: didn't find _parameters field in the message.");
        }
    }
}

InspectorTest.waitUntilMessageReceived = function(callback)
{
    InspectorTest.addSniffer(WebInspector.consoleModel, "addMessage", callback, false);
}

InspectorTest.waitUntilNthMessageReceived = function(count, callback)
{
    function override()
    {
        if (--count === 0)
            InspectorTest.safeWrap(callback)();
        else
            InspectorTest.addSniffer(WebInspector.consoleModel, "addMessage", override, false);
    }
    InspectorTest.addSniffer(WebInspector.consoleModel, "addMessage", override, false);
}

InspectorTest.changeExecutionContext = function(namePrefix)
{
    var selector = WebInspector.ConsolePanel._view()._executionContextSelector._selectElement;
    var option = selector.firstChild;
    while (option) {
        if (option.textContent && option.textContent.startsWith(namePrefix))
            break;
        option = option.nextSibling;
    }
    if (!option) {
        InspectorTest.addResult("FAILED: context with prefix: "  + namePrefix + " not found in the context list");
        return;
    }
    option.selected = true;
    WebInspector.ConsolePanel._view()._executionContextChanged();
}

}
