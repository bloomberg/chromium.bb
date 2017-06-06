<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script src="resources/log-source.js"></script>
<script>
// Create a mix of log messages from different source files
function onload()
{
   console.info("sample info");
   console.log("sample log");
   console.warn("sample warning");
   console.debug("sample debug");
   console.error("sample error");
   
   console.info("abc info");
   console.info("def info");
   
   console.warn("abc warn");
   console.warn("def warn");
   
   runTest();
}
//# sourceURL=console-filter-level-test.html
</script>

<script>
function test()
{
    function dumpVisibleMessages()
    {
        var menuText = Console.ConsoleView.instance()._filter._levelMenuButton.text();
        InspectorTest.addResult("Level menu: " + menuText);

        var messages = Console.ConsoleView.instance()._visibleViewMessages;
        for (var i = 0; i < messages.length; i++)
            InspectorTest.addResult(">" + messages[i].toMessageElement().deepTextContent());
    }

    var testSuite = [
        function dumpLevels(next) {
            InspectorTest.addResult('All levels');
            InspectorTest.addObject(Console.ConsoleViewFilter.allLevelsFilterValue());
            InspectorTest.addResult('Default levels');
            InspectorTest.addObject(Console.ConsoleViewFilter.defaultLevelsFilterValue());
            next();
        },

        function beforeFilter(next)
        {
            InspectorTest.addResult(arguments.callee.name);
            dumpVisibleMessages();
            next();
        },

        function allLevels(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleViewFilter.allLevelsFilterValue());
            dumpVisibleMessages();
            next();
        },

        function defaultLevels(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set(Console.ConsoleViewFilter.defaultLevelsFilterValue());
            dumpVisibleMessages();
            next();
        },

        function verbose(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set({verbose: true});
            dumpVisibleMessages();
            next();
        },
        
        function info(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set({'info': true});
            dumpVisibleMessages();
            next();
        },

        function warningsAndErrors(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set({'warning': true, 'error': true});
            dumpVisibleMessages();
            next();
        },

        function abcMessagePlain(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set({verbose: true});
            Console.ConsoleView.instance()._filter._textFilterUI.setValue("abc");
            Console.ConsoleView.instance()._filter._textFilterChanged();
            dumpVisibleMessages();
            next();
        },
        
        function abcMessageRegex(next)
        {
            Console.ConsoleView.instance()._filter._textFilterUI.setValue("/ab[a-z]/");
            Console.ConsoleView.instance()._filter._textFilterChanged();
            dumpVisibleMessages();
            next();
        },

        function abcMessageRegexWarning(next)
        {
            Console.ConsoleViewFilter.levelFilterSetting().set({'warning': true});
            dumpVisibleMessages();
            next();
        }
    ]

    InspectorTest.evaluateInConsole("'Should be always visible'", InspectorTest.runTestSuite.bind(InspectorTest, testSuite));
}

</script>
</head>

<body onload="onload()">
<p>
    Tests that console can filter messages by source.
</p>

</body>
</html>
