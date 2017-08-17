<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function populateConsoleWithMessages()
{
    for (var i = 0; i < 200; ++i)
        console.log("Message #" + i);
    console.log("LAST MESSAGE");
    runTest();
}

function test()
{
    var consoleView = Console.ConsoleView.instance();
    var viewport = consoleView._viewport;
    const maximumViewportMessagesCount = 150;
    InspectorTest.runTestSuite([
        function waitForMessages(next)
        {
            // NOTE: keep in sync with populateConsoleWithMessages above.
            const expectedMessageCount = 201;
            InspectorTest.waitForConsoleMessages(expectedMessageCount, next);
        },

        function verifyViewportIsTallEnough(next)
        {
            viewport.invalidate();
            var viewportMessagesCount = viewport._lastVisibleIndex - viewport._firstVisibleIndex;
            if (viewportMessagesCount > maximumViewportMessagesCount) {
                InspectorTest.addResult(String.sprintf("Test cannot be run because viewport could fit %d messages which is more than maximum of %d.", viewportMessagesCount, maximumViewportMessagesCount));
                InspectorTest.completeTest();
                return;
            }
            next();
        },

        function scrollConsoleToTop(next)
        {
            viewport.forceScrollItemToBeFirst(0);
            dumpTop();
            next();
        },

        function testFindLastMessage(next)
        {
            InspectorTest.addSniffer(consoleView, "_searchFinishedForTests", callback);
            consoleView._searchableView._searchInputElement.value = "LAST MESSAGE";
            consoleView._searchableView.showSearchField();

            function callback()
            {
                consoleView._searchableView.handleFindNextShortcut();
                dumpBottom();
                next();
            }
        }
    ]);

    function dumpTop()
    {
        viewport.refresh();
        InspectorTest.addResult("first visible message index: " + viewport.firstVisibleIndex());
    }

    function dumpBottom()
    {
        viewport.refresh();
        InspectorTest.addResult("last visible message index: " + viewport.lastVisibleIndex());
    }
}
</script>
</head>
<body onload="populateConsoleWithMessages()">
<p>
    Tests that console viewport reveals messages on searching.
</p>
</body>
</html>
