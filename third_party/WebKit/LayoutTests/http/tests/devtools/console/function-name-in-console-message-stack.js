<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
var foo = function ()
{
    throw new Error();
}

foo.displayName = 'foo.displayName';
Object.defineProperty(foo, 'name', { value: 'foo.function.name' } );

var bar = function()
{
    foo();
}

bar.displayName = 'bar.displayName';

var baz = function()
{
    bar();
}

Object.defineProperty(baz, 'name', { value: 'baz.function.name' } );

function test()
{
    InspectorTest.waitUntilNthMessageReceived(1, step1);
    InspectorTest.evaluateInPage("setTimeout(baz, 0);");

    function step1()
    {
        InspectorTest.expandConsoleMessages(step2);
    }

    function step2()
    {
        InspectorTest.dumpConsoleMessagesIgnoreErrorStackFrames();
        InspectorTest.completeTest();
    }
};
</script>
</head>
<body onload="runTest()">
<p>Tests exception message contains stack with correct function name.</p>
</body>
</html>
