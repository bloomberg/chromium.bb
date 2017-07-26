var initialize_ConsoleTest = function() {

InspectorTest.preloadModule("source_frame");
InspectorTest.preloadPanel("console");

InspectorTest.selectMainExecutionContext = function()
{
    var executionContexts = InspectorTest.runtimeModel.executionContexts();
    for (var context of executionContexts) {
        if (context.isDefault) {
            UI.context.setFlavor(SDK.ExecutionContext, context);
            return;
        }
    }
}

InspectorTest.evaluateInConsole = function(code, callback, dontForceMainContext)
{
    if (!dontForceMainContext)
        InspectorTest.selectMainExecutionContext();
    callback = InspectorTest.safeWrap(callback);

    var consoleView = Console.ConsoleView.instance();
    consoleView._prompt._appendCommand(code, true);
    InspectorTest.addConsoleViewSniffer(function(commandResult) {
        callback(commandResult.toMessageElement().deepTextContent());
    });
}

InspectorTest.addConsoleViewSniffer = function(override, opt_sticky)
{
    var sniffer = function (viewMessage) {
        override(viewMessage);
    };

    InspectorTest.addSniffer(Console.ConsoleView.prototype, "_consoleMessageAddedForTest", sniffer, opt_sticky);
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
    // Remove line and column of evaluate method.
    messageText = messageText.replace(/(at eval \(eval at evaluate) \(:\d+:\d+\)/, '$1');

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
    var viewport = Console.ConsoleView.instance()._viewport;
    viewport.element.style.width = width + "px";
    viewport.element.style.height = height + "px";
    viewport.element.style.position = "absolute";
    viewport.invalidate();
}

InspectorTest.consoleMessagesCount = function()
{
    var consoleView = Console.ConsoleView.instance();
    return consoleView._consoleMessages.length;
}

InspectorTest.dumpConsoleMessages = function(printOriginatingCommand, dumpClassNames, formatter)
{
    InspectorTest.addResults(InspectorTest.dumpConsoleMessagesIntoArray(printOriginatingCommand, dumpClassNames, formatter));
}

InspectorTest.dumpConsoleMessagesIntoArray = function(printOriginatingCommand, dumpClassNames, formatter)
{
    Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleViewFilter.allLevelsFilterValue());
    formatter = formatter || InspectorTest.prepareConsoleMessageText;
    var result = [];
    InspectorTest.disableConsoleViewport();
    var consoleView = Console.ConsoleView.instance();
    if (consoleView._needsFullUpdate)
        consoleView._updateMessageList();
    var viewMessages = consoleView._visibleViewMessages;
    for (var i = 0; i < viewMessages.length; ++i) {
        var uiMessage = viewMessages[i];
        var message = uiMessage.consoleMessage();
        var element = uiMessage.element();

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
        Console.ConsoleView.instance()._viewport.invalidate();
    var table = viewMessage.element();
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
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].element();
        var messageText = InspectorTest.prepareConsoleMessageText(element);
        InspectorTest.addResult(messageText);
        var spans = element.querySelectorAll(".console-message-text *");
        for (var j = 0; j < spans.length; ++j)
            InspectorTest.addResult("Styled text #" + j + ": " + (spans[j].style.cssText || "NO STYLES DEFINED"));
    }
}

InspectorTest.dumpConsoleMessagesWithClasses = function(sortMessages) {
    var result = [];
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].element();
        var contentElement = messageViews[i].contentElement();
        var messageText = InspectorTest.prepareConsoleMessageText(element);
        result.push(messageText + " " + element.getAttribute("class") + " > " + contentElement.getAttribute("class"));
    }
    if (sortMessages)
        result.sort();
    for (var i = 0; i < result.length; ++i)
        InspectorTest.addResult(result[i]);
}

InspectorTest.dumpConsoleClassesBrief = function()
{
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i)
        InspectorTest.addResult(messageViews[i].toMessageElement().className);
}

InspectorTest.dumpConsoleCounters = function()
{
    var counter = ConsoleCounters.WarningErrorCounter._instanceForTest;
    for (var index = 0; index < counter._titles.length; ++index)
        InspectorTest.addResult(counter._titles[index]);
    InspectorTest.dumpConsoleClassesBrief();
}

InspectorTest.expandConsoleMessages = function(callback, deepFilter, sectionFilter)
{
    Console.ConsoleView.instance()._invalidateViewport();
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;

    // Initiate round-trips to fetch necessary data for further rendering.
    for (var i = 0; i < messageViews.length; ++i)
        messageViews[i].element();

    InspectorTest.deprecatedRunAfterPendingDispatches(expandTreeElements);

    function expandTreeElements()
    {
        for (var i = 0; i < messageViews.length; ++i) {
            var element = messageViews[i].element();
            for (var node = element; node; node = node.traverseNextNode(element)) {
                if (node.treeElementForTest)
                    node.treeElementForTest.expand();
                if (node._expandStackTraceForTest)
                    node._expandStackTraceForTest();
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
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    var properties = [];
    var propertiesCount  = 0;
    InspectorTest.addSniffer(ObjectUI.ObjectPropertyTreeElement.prototype, "_updateExpandable", propertyExpandableUpdated);
    for (var i = 0; i < messageViews.length; ++i) {
        var element = messageViews[i].element();
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
            InspectorTest.addSniffer(ObjectUI.ObjectPropertyTreeElement.prototype, "_updateExpandable", propertyExpandableUpdated);
        }
    }
}

InspectorTest.expandConsoleMessagesErrorParameters = function(callback)
{
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    // Initiate round-trips to fetch necessary data for further rendering.
    for (var i = 0; i < messageViews.length; ++i)
        messageViews[i].element();
    InspectorTest.deprecatedRunAfterPendingDispatches(callback);
}

InspectorTest.waitForRemoteObjectsConsoleMessages = function(callback)
{
    var messages = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messages.length; ++i)
        messages[i].toMessageElement();
    InspectorTest.deprecatedRunAfterPendingDispatches(callback);
}

InspectorTest.checkConsoleMessagesDontHaveParameters = function()
{
    var messageViews = Console.ConsoleView.instance()._visibleViewMessages;
    for (var i = 0; i < messageViews.length; ++i) {
        var m = messageViews[i].consoleMessage();
        InspectorTest.addResult("Message[" + i + "]:");
        InspectorTest.addResult("Message: " + Bindings.displayNameForURL(m.url) + ":" + m.line + " " + m.message);
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

InspectorTest.waitUntilConsoleEditorLoaded = function()
{
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    var editor = Console.ConsoleView.instance()._prompt._editor;
    if (editor)
        fulfill(editor);
    else
        InspectorTest.addSniffer(Console.ConsolePrompt.prototype, "_editorSetForTest", _ => fulfill(editor))
    return promise;
}

InspectorTest.waitUntilMessageReceived = function(callback)
{
    InspectorTest.addSniffer(InspectorTest.consoleModel, "addMessage", callback, false);
}

InspectorTest.waitUntilMessageReceivedPromise = function()
{
    var callback;
    var promise = new Promise((fullfill) => callback = fullfill);
    InspectorTest.waitUntilMessageReceived(callback);
    return promise;
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

InspectorTest.waitUntilNthMessageReceivedPromise = function(count)
{
    var callback;
    var promise = new Promise((fullfill) => callback = fullfill);
    InspectorTest.waitUntilNthMessageReceived(count, callback);
    return promise;
}

InspectorTest.changeExecutionContext = function(namePrefix)
{
    var selector = Console.ConsoleView.instance()._consoleContextSelector;
    for (var executionContext of selector._items) {
        if (selector.titleFor(executionContext).startsWith(namePrefix)) {
            UI.context.setFlavor(SDK.ExecutionContext, executionContext);
            return;
        }
    }
    InspectorTest.addResult("FAILED: context with prefix: "  + namePrefix + " not found in the context list");
}

InspectorTest.waitForConsoleMessages = function(expectedCount, callback)
{
    var consoleView = Console.ConsoleView.instance();
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

InspectorTest.selectConsoleMessages = function(fromMessage, fromTextOffset, toMessage, toTextOffset, useTextContainer)
{
    var consoleView = Console.ConsoleView.instance();
    var from = selectionContainerAndOffset(consoleView.itemElement(fromMessage).element(), fromTextOffset);
    var to = selectionContainerAndOffset(consoleView.itemElement(toMessage).element(), toTextOffset);
    window.getSelection().setBaseAndExtent(from.container, from.offset, to.container, to.offset);

    function selectionContainerAndOffset(container, offset)
    {
        if (offset === 0 && container.nodeType !== Node.TEXT_NODE)
            container = container.traverseNextTextNode();
        var charCount = 0;
        var node = container;
        while (node = node.traverseNextTextNode(true)) {
            var length = node.textContent.length;
            if (charCount + length >= offset) {
                return {
                    container: node,
                    offset: offset - charCount
                };
            }
            charCount += length;
        }
        return null;
    }
}

}
