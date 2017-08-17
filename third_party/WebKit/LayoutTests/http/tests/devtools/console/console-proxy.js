<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

window.accessedGet = false;
function testFunction()
{
    let proxied = new Proxy({ boo: 42 }, {
        get: function (target, name, receiver) {
            window.accessedGet = true;
            return target[name];
        },
        set: function(target, name, value, receiver) {
            target[name] = value;
            return value;
        }
    });
    proxied.foo = 43;
    console.log(proxied);
    let proxied2 = new Proxy(proxied, {});
    console.log(proxied2);
}

function test()
{
    InspectorTest.waitUntilNthMessageReceived(2, dumpMessages);
    InspectorTest.evaluateInPage("testFunction()");

    function dumpMessages()
    {
        var consoleView = Console.ConsoleView.instance();
        consoleView._viewport.invalidate()
        var element = consoleView._visibleViewMessages[0].contentElement();

        InspectorTest.dumpConsoleMessages(false, true);
        InspectorTest.evaluateInPage("window.accessedGet", dumpAccessedGetAndExpand);
    }

    function dumpAccessedGetAndExpand(result)
    {
        InspectorTest.addResult("window.accessedGet = " + result.value);
        InspectorTest.expandConsoleMessages(dumpExpandedConsoleMessages);
    }

    function dumpExpandedConsoleMessages()
    {
        var element = Console.ConsoleView.instance()._visibleViewMessages[0].contentElement();
        dumpNoteVisible(element, "info-note");

        InspectorTest.dumpConsoleMessages(false, true);
        InspectorTest.evaluateInPage("window.accessedGet", dumpAccessedGetAndCompleteTest);
    }

    function dumpAccessedGetAndCompleteTest(result)
    {
        InspectorTest.addResult("window.accessedGet = " + result.value);
        InspectorTest.completeTest();
    }

    function dumpNoteVisible(element, name)
    {
        var note = window.getComputedStyle(element.querySelector('.object-state-note.' + name)).display;
        InspectorTest.addResult(name + " display: " + note);
    }
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that console logging dumps proxy properly.
</p>

</body>
</html>
