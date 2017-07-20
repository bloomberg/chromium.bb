<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>

<script>
function log(data)
{
    console.log(data);
}

var a0 = [];
var a1 = []; a1.length = 1;
var a2 = []; a2.length = 5;
var a3 = [,2,3];
var a4 = []; a4.length = 15;
var a5 = []; a5.length = 15; a5[8] = 8;
var a6 = []; a6.length = 15; a6[0] = 0; a6[10] = 10;
var a7 = [,,,4]; a7.length = 15;
for (var i = 0; i < 6; ++i)
    a7["index" + i] = i;
var a8 = [];
for (var i = 0; i < 10; ++i)
    a8[i] = i;
var a9 = [];
for (var i = 1; i < 5; ++i) {
    a9[i] = i;
    a9[i + 5] = i + 5;
}
a9.length = 11;
a9.foo = "bar";
a10 = Object.create([1,2]);
//# sourceURL=console-format-array-prototype.html
</script>

<script>
function test()
{
    loopOverGlobals(0, 11);

    function loopOverGlobals(current, total)
    {
        function advance()
        {
            var next = current + 1;
            if (next === total) {
                InspectorTest.evaluateInPage("tearDown()");
                InspectorTest.deprecatedRunAfterPendingDispatches(finish);
            } else {
                loopOverGlobals(next, total);
            }
        }

        function finish()
        {
            InspectorTest.dumpConsoleMessages(false, false, InspectorTest.textContentWithLineBreaks);
            InspectorTest.completeTest();
        }

        InspectorTest.evaluateInConsole("a" + current);
        InspectorTest.deprecatedRunAfterPendingDispatches(invokeConsoleLog);
        function invokeConsoleLog()
        {
            InspectorTest.evaluateInPage("log(a" + current + ")");
            InspectorTest.deprecatedRunAfterPendingDispatches(advance);
        }
    }
}
</script>
</head>

<body onload="runTest()">
<p>
Tests that console logging dumps array values defined on Array.prototype[].
</p>
</body>
</html>
