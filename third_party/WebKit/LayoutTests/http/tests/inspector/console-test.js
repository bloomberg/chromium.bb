var initialize_ConsoleTest = function() {

InspectorTest.preloadModule("source_frame");
InspectorTest.preloadPanel("console");

InspectorTest.selectMainExecutionContext = function()
{
    var executionContexts = InspectorTest.mainTarget.runtimeModel.executionContexts();
    for (var context of executionContexts) {
        if (context.isDefault) {
            WebInspector.context.setFlavor(WebInspector.ExecutionContext, context);
            return;
        }
    }
}

InspectorTest.evaluateInConsole = function(code, callback, dontForceMainContext)
{
    if (!dontForceMainContext)
        InspectorTest.selectMainExecutionContext();
    callback = InspectorTest.safeWrap(callback);

    var consoleView = WebInspector.ConsoleView.instance();
    consoleView.visible = true;
    consoleView._prompt.setText(code);
    var event = document.createEvent("KeyboardEvent");
    event.initKeyboardEvent("keydown", true, true, null, "Enter", "");
    consoleView._prompt.proxyElementForTests().dispatchEvent(event);
    InspectorTest.addConsoleViewSniffer(function(commandResult) {
        callback(commandResult.toMessageElement().deepTextContent());
    });
}

InspectorTest.addConsoleViewSniffer = function(override, opt_sticky)
{
    var sniffer = function (viewMessage) {
        override(viewMessage);
    };

    InspectorTest.addSniffer(WebInspector.ConsoleView.prototype, "_consoleMessageAddedForTest", sniffer, opt_sticky);
}

InspectorTest.evaluateInConsoleAndDump = function(code, callback, dontForceMainContext)
{
    callback = InspectorTest.safeWrap(callback);

    function mycallback(text)
    {
        text = text.replace(/\bVM\d+/g, "VM");
        InspectorTest.addResult(code + " = " + text);
        callback(text);
    }
    InspectorTest.evaluateInConsole(code, mycallback, dontForceMainContext);
}

InspectorTest.prepareConsoleMessageText = function(messageElement, consoleMessage)
{
    var messageText = messageElement.deepTextContent().replace(/\u200b/g, "");
    // Replace scriptIds with generic scriptId string to avoid flakiness.
    messageText = messageText.replace(/VM\d+/g, "VM");
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
    var viewport = WebInspector.ConsoleView.instance()._viewport;
    viewport.element.style.width = width + "px";
    viewport.element.style.height = height + "px";
    viewport.element.style.position = "absolute";
    viewport.invalidate();
}

InspectorTest.consoleMessagesCount = function()
{
    var consoleView = WebInspector.ConsoleView.instance();
    return consoleView._consoleMessages.length;
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
    var consoleView = WebInspector.ConsoleView.instance();
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
                    classNames.push(node.className.replace("platform-linux", "platform-*").replace("platform-mac", "platform-*").replace("platform-windows", "platform-*"));
            }
        }

        if (InspectorTest.dumpConsoleTableMessage(uiMessage, false, result)) {
            if (dumpClassNames)
                result.push(classNames.join(" > "));
        } else {
            var messageText = formatter(element, message);
            messageText = messageText.replace(/VM\d+/g, "VM");
            result.push(messageText + (dumpClassNames ? " " + classNames.join(" > ") : ""));
        }

        if (printOriginatingCommand && uiMessage.consoleMessage().originatingMessage())
            result.push("Originating from: " + uiMessage.consoleMessage().originatingMessage().messageText);
    }
    return result;
}

InspectorTest.formatterIgnoreStackFrameUrls = function(messageFormatter, node)
{
    function isNotEmptyLine(string)
    {
        return string.trim().length > 0;
    }

    function ignoreStackFrameAndMutableData(string)
    {
        var buffer = string.replace(/\u200b/g, "");
        buffer = buffer.replace(/VM\d+/g, "VM");
        return buffer.replace(/^\s+at [^\]]+(]?)$/, "$1");
    }

    messageFormatter = messageFormatter || InspectorTest.textContentWithLineBreaks;
    var buffer = messageFormatter(node);
    return buffer.split("\n").map(ignoreStackFrameAndMutableData).filter(isNotEmptyLine).join("\n");
}

InspectorTest.simpleFormatter = function(element, message)
{
    return message.messageText + ":" + message.line + ":" + message.column;
}

InspectorTest.dumpConsoleMessagesIgnoreErrorStackFrames = function(printOriginatingCommand, dumpClassNames, messageFormatter)
{
    InspectorTest.addResults(InspectorTest.dumpConsoleMessagesIntoArray(printOriginatingCommand, dumpClassNames, InspectorTest.formatterIgnoreStackFrameUrls.bind(this, messageFormatter)));
}

InspectorTest.dumpConsoleTableMessage = function(viewMessage, forceInvalidate, results)
{
    if (forceInvalidate)
        WebInspector.ConsoleView.instance()._viewport.invalidate();
    var table = viewMessage.contentElement();
    var headers = table.querySelectorAll("th > div:first-child");
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
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].contentElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element)
        InspectorTest.addResult(messageText);
        var spans = element.querySelectorAll(".console-message-text > span *");
        for (var j = 0; j < spans.length; ++j)
            InspectorTest.addResult("Styled text #" + j + ": " + (spans[j].style.cssText || "NO STYLES DEFINED"));
    }
}

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    var result = [];
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].contentElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element);
        result.push(messageText + " " + messageViews[i].toMessageElement().getAttribute("class") + " > " + element.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < result.length; ++i)
        InspectorTest.addResult(result[i]);
}

InspectorTest.dumpConsoleClassesBrief = function()
{
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i)
        InspectorTest.addResult(messageViews[i].toMessageElement().className);
}

InspectorTest.dumpConsoleCounters = function()
{
    var counter = WebInspector.Main.WarningErrorCounter._instanceForTest;
    for (var index = 0; index < counter._titles.length; ++index)
        InspectorTest.addResult(counter._titles[index]);
    InspectorTest.dumpConsoleClassesBrief();
}

InspectorTest.expandConsoleMessages = function(callback, deepFilter, sectionFilter)
{
    WebInspector.ConsoleView.instance()._viewportThrottler.flush();
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;

    // Initiate round-trips to fetch necessary data for further rendering.
    for (var i = 0; i < messageViews.length; ++i)
        messageViews[i].contentElement();

    InspectorTest.deprecatedRunAfterPendingDispatches(expandTreeElements);

    function expandTreeElements()
    {
        for (var i = 0; i < messageViews.length; ++i) {
            var element = messageViews[i].contentElement();
            for (var node = element; node; node = node.traverseNextNode(element)) {
                if (node.treeElementForTest)
                    node.treeElementForTest.expand();
                if (!node._section)
                    continue;
                if (sectionFilter && !sectionFilter(node._section))
                    continue;
                node._section.expand();

                if (!deepFilter)
                    continue;
                var treeElements = node._section.rootElement().children();
                for (var j = 0; j < treeElements.length; ++j) {
                    for (var treeElement = treeElements[j]; treeElement; treeElement = treeElement.traverseNextTreeElement(true, null, true)) {
                        if (deepFilter(treeElement))
                            treeElement.expand();
                    }
                }
            }
        }
        InspectorTest.deprecatedRunAfterPendingDispatches(callback);
    }
}

InspectorTest.expandGettersInConsoleMessages = function(callback)
{
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
    var properties = [];
    var propertiesCount  = 0;
    InspectorTest.addSniffer(WebInspector.ObjectPropertyTreeElement.prototype, "_updateExpandable", propertyExpandableUpdated);
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].contentElement();
        for (var node = element; node; node = node.traverseNextNode(element)) {
            if (node.classList && node.classList.contains("object-value-calculate-value-button")) {
                ++propertiesCount;
                node.click();
                properties.push(node.parentElement.parentElement);
            }
        }
    }

    function propertyExpandableUpdated()
    {
        --propertiesCount;
        if (propertiesCount === 0) {
            for (var i = 0; i < properties.length; ++i)
                properties[i].click();
            InspectorTest.deprecatedRunAfterPendingDispatches(callback);
        } else {
            InspectorTest.addSniffer(WebInspector.ObjectPropertyTreeElement.prototype, "_updateExpandable", propertyExpandableUpdated);
        }
    }
}

InspectorTest.expandConsoleMessagesErrorParameters = function(callback)
{
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
    // Initiate round-trips to fetch necessary data for further rendering.
    for (var i = 0; i < messageViews.length; ++i)
        messageViews[i].contentElement();
    InspectorTest.deprecatedRunAfterPendingDispatches(expandErrorParameters);
    function expandErrorParameters()
    {
        for (var i = 0; i < messageViews.length; ++i) {
            var element = messageViews[i].contentElement();
            var spans = element.querySelectorAll("span.object-value-error");
            for (var j = 0; j < spans.length; ++j) {
                var links = spans[j].querySelectorAll("a");
                for (var k = 0; k < links.length; ++k) {
                    var link = links[k];
                    if (link && link._showDetailedForTest)
                        link._showDetailedForTest();
                }
            }
        }
        callback();
    }
}

InspectorTest.waitForRemoteObjectsConsoleMessages = function(callback)
{
    var messages = WebInspector.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messages.length; ++i)
        messages[i].toMessageElement();
    InspectorTest.deprecatedRunAfterPendingDispatches(callback);
}

InspectorTest.checkConsoleMessagesDontHaveParameters = function()
{
    var messageViews = WebInspector.ConsoleView.instance()._visibleViewMessages;
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
    InspectorTest.addSniffer(InspectorTest.consoleModel, "addMessage", callback, false);
}

InspectorTest.waitUntilNthMessageReceived = function(count, callback)
{
    function override()
    {
        if (--count === 0)
            InspectorTest.safeWrap(callback)();
        else
            InspectorTest.addSniffer(InspectorTest.consoleModel, "addMessage", override, false);
    }
    InspectorTest.addSniffer(InspectorTest.consoleModel, "addMessage", override, false);
}

InspectorTest.changeExecutionContext = function(namePrefix)
{
    var selector = WebInspector.ConsoleView.instance()._executionContextModel._selectElement;
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
    WebInspector.ConsoleView.instance()._executionContextModel._executionContextChanged();
}

InspectorTest.waitForConsoleMessages = function(expectedCount, callback)
{
    var consoleView = WebInspector.ConsoleView.instance();
    checkAndReturn();

    function checkAndReturn()
    {
        if (consoleView._visibleViewMessages.length === expectedCount) {
            InspectorTest.addResult("Message count: " + expectedCount);
            callback();
        } else {
            InspectorTest.addSniffer(consoleView, "_messageAppendedForTests", checkAndReturn);
        }
    }
}

}
