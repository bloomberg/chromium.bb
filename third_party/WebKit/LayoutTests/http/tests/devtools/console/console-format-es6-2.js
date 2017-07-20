<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

var globals = [];

function log(current)
{
    console.log(globals[current]);
    console.log([globals[current]]);
}

function onload()
{
    var map2 = new Map();
    map2.set(41, 42);
    map2.set({foo: 1}, {foo: 2});

    var iter1 = map2.values();
    iter1.next();

    var set2 = new Set();
    set2.add(41);
    set2.add({foo: 1});

    var iter2 = set2.keys();
    iter2.next();

    globals = [
        map2.keys(), map2.values(), map2.entries(),
        set2.keys(), set2.values(), set2.entries(),
        iter1, iter2,
    ];

    runTest();
}

function test()
{
    InspectorTest.evaluateInPage("globals.length", loopOverGlobals.bind(this, 0));

    function loopOverGlobals(current, total)
    {
        function advance()
        {
            var next = current + 1;
            if (next == total.description)
                finish();
            else
                loopOverGlobals(next, total);
        }

        function finish()
        {
            InspectorTest.dumpConsoleMessages(false, false, InspectorTest.textContentWithLineBreaks);
            InspectorTest.addResult("Expanded all messages");
            InspectorTest.expandConsoleMessages(dumpConsoleMessages);
        }

        function dumpConsoleMessages()
        {
            InspectorTest.dumpConsoleMessages(false, false, InspectorTest.textContentWithLineBreaks);
            InspectorTest.completeTest();
        }

        InspectorTest.evaluateInPage("log(" + current + ")");
        InspectorTest.deprecatedRunAfterPendingDispatches(evalInConsole);
        function evalInConsole()
        {
            InspectorTest.evaluateInConsole("globals[" + current + "]");
            InspectorTest.deprecatedRunAfterPendingDispatches(advance);
        }
    }
}
</script>
</head>

<body onload="onload()">
<p>
Tests that console properly displays information about ES6 features.
</p>
</body>
</html>
