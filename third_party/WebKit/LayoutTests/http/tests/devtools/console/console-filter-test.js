<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script src="resources/log-source.js"></script>
<script>
function log1()
{
    console.log.apply(console, arguments);
}

// Create a mix of log messages from different source files
function onload()
{
    for (var i = 0; i < 10; i++) {
        if (i % 2 == 0)
            log1(i + "topGroup"); // from console-filter-test.html
        else
            log2(i + "topGroup"); // from log-source.js
    }

    console.group("outerGroup");
    for (var i = 10; i < 20; i++) {
        if (i % 2 == 0)
            log1(i + "outerGroup"); // from console-filter-test.html
        else
            log2(i + "outerGroup"); // from log-source.js
    }
    console.group("innerGroup");
    for (var i = 20; i < 30; i++) {
        if (i % 2 == 0)
            log1(i + "innerGroup"); // from console-filter-test.html
        else
            log2(i + "innerGroup"); // from log-source.js
    }
    console.groupEnd();
    console.groupEnd();

    var logger1 = console.context("context1");
    var logger2 = console.context("context2");
    logger1.log("Hello 1");
    logger2.log("Hello 2");

    console.log("end");

    runTest();
}

function test()
{
    var messages = Console.ConsoleView.instance()._visibleViewMessages;

    function dumpVisibleMessages()
    {
        var messages = Console.ConsoleView.instance()._visibleViewMessages;
        for (var i = 0; i < messages.length; ++i) {
            var viewMessage = messages[i];
            var delimeter = viewMessage.consoleMessage().isGroupStartMessage() ? ">" : "";
            var indent = "";
            for (var j = 0; j < viewMessage.nestingLevel(); ++j)
                indent += "  ";
            InspectorTest.addResult(indent + delimeter + viewMessage.toMessageElement().deepTextContent());
        }
    }

    var url1 = messages[0].consoleMessage().url;
    var url2 = messages[1].consoleMessage().url;

    InspectorTest.runTestSuite([
        function beforeFilter(next)
        {
            InspectorTest.addResult(arguments.callee.name);
            dumpVisibleMessages();
            next();
        },
        function addURL1Filter(next)
        {
            Console.ConsoleView.instance()._filter.addMessageURLFilter(url1);
            dumpVisibleMessages();
            next();
        },
        function addURL2Filter(next)
        {
            Console.ConsoleView.instance()._filter.addMessageURLFilter(url2);
            dumpVisibleMessages();
            next();
        },
        function removeURL1Filter(next)
        {
            Console.ConsoleView.instance()._filter.removeMessageURLFilter(url1);
            dumpVisibleMessages();
            next();
        },
        function restoreURL1Filter(next)
        {
            Console.ConsoleView.instance()._filter.addMessageURLFilter(url1);
            dumpVisibleMessages();
            next();
        },
        function removeAllFilters(next)
        {
            Console.ConsoleView.instance()._filter.removeMessageURLFilter();
            dumpVisibleMessages();
            next();
        },
        function checkContextFilter(next)
        {
            Console.ConsoleView.instance()._filter.setContext("context1");
            dumpVisibleMessages();
            next();
        },
        function checkAllContextsFilter(next)
        {
            Console.ConsoleView.instance()._filter.setContext(Console.ConsoleSidebar.AllContextsFilter);
            dumpVisibleMessages();
            next();
        },
        function checkTextFilter(next)
        {
            Console.ConsoleView.instance()._filter._textFilterUI.setValue("outer");
            Console.ConsoleView.instance()._filter._textFilterChanged();
            dumpVisibleMessages();
            next();
        },
        function checkResetFilter(next)
        {
            Console.ConsoleView.instance()._filter.reset();
            dumpVisibleMessages();
            next();
        }
    ]);
}

</script>
</head>

<body onload="onload()">
<p>
    Tests that console can filter messages by source.
</p>

</body>
</html>
