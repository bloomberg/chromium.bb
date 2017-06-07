<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
console.log("http://www.chromium.org/");
console.log("follow http://www.chromium.org/");
console.log("string", "http://www.chromium.org/");
console.log(123, "http://www.chromium.org/");
console.log("http://www.chromium.org/some?v=114:56:57");


function test()
{
    InspectorTest.dumpConsoleMessages(false, true);

    InspectorTest.addResult("Dump urls in messages");
    var consoleView = Console.ConsoleView.instance();
    var viewMessages = consoleView._visibleViewMessages;
    for (var i = 0; i < viewMessages.length; ++i) {
        var uiMessage = viewMessages[i];
        var element = uiMessage.contentElement();
        var links = element.querySelectorAll(".devtools-link");
        for (var link of links) {
            var info = Components.Linkifier._linkInfo(link);
            InspectorTest.addResult("linked url:" + (info && info.url));
        }
    }

    var linkifyMe = "at triggerError (http://localhost/show/:22:11)";
    var fragment = Console.ConsoleViewMessage._linkifyStringAsFragment(linkifyMe);
    var anchor = fragment.querySelector('.devtools-link');
    var info = Components.Linkifier._linkInfo(anchor);
    InspectorTest.addResult("The string \"" + linkifyMe + " \" linkifies to url: " + (info && info.url));
    InspectorTest.addResult("The lineNumber is " + (info && info.lineNumber));
    InspectorTest.addResult("The columnNumber is " + (info && info.columnNumber));

    // Ensures urls with lots of slashes does not bog down the regex.
    Console.ConsoleViewMessage._linkifyStringAsFragment("/".repeat(1000));
    Console.ConsoleViewMessage._linkifyStringAsFragment("/a/".repeat(1000));

    InspectorTest.completeTest();
}
</script>
</head>
<body onload="runTest()">
<p>
Test that console.log() would linkify the links. <a href="http://crbug.com/231074">Bug 231074.</a>
</p>
</body>
</html>
