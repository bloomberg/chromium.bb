<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function foo()
{
    console.trace(239);
}
//# sourceURL=foo.js
</script>
<script>
function boo()
{
    foo();
}
//# sourceURL=boo.js
</script>
<script>

function test()
{
    InspectorTest.evaluateInPage("boo()", step1);

    function step1()
    {
        dumpConsoleMessageURLs();

        InspectorTest.addSniffer(Bindings.BlackboxManager.prototype, "_patternChangeFinishedForTests", step2);
        var frameworkRegexString = "foo\\.js";
        Common.settingForTest("skipStackFramesPattern").set(frameworkRegexString);
    }

    function step2()
    {
        dumpConsoleMessageURLs();
        InspectorTest.addSniffer(Bindings.BlackboxManager.prototype, "_patternChangeFinishedForTests", step3);
        var frameworkRegexString = "foo\\.js|boo\\.js";
        Common.settingForTest("skipStackFramesPattern").set(frameworkRegexString);
    }

    function step3()
    {
        dumpConsoleMessageURLs();
        InspectorTest.addSniffer(Bindings.BlackboxManager.prototype, "_patternChangeFinishedForTests", step4);
        var frameworkRegexString = "";
        Common.settingForTest("skipStackFramesPattern").set(frameworkRegexString);
    }

    function step4()
    {
        dumpConsoleMessageURLs();
        InspectorTest.completeTest();
    }

    function dumpConsoleMessageURLs()
    {
        var messages = Console.ConsoleView.instance()._visibleViewMessages;
        for (var i = 0; i < messages.length; ++i) {
            var element = messages[i].toMessageElement();
            var anchor = element.querySelector(".console-message-anchor");
            InspectorTest.addResult(anchor.textContent.replace(/VM\d+/g, "VM"));
        }
    }
}
</script>
</head>
<body onload="runTest()">
<p>
Test that console.log() would linkify its location in respect with blackboxing.
</p>
</body>
</html>
