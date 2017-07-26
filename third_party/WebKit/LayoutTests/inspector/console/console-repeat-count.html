<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function dumpMessages()
{
    for (var i = 0; i < 2; ++i)
        console.log("Message");

    for (var i = 0; i < 2; ++i)
        console.log(new Error("Message with error"));

    for (var i = 0; i < 2; ++i)
        console.error({a: 1});
}

function throwObjects() {
    for (var i = 0; i < 2; ++i)
        setTimeout(() => { throw {a: 1}; }, 0);
}

function throwPrimitiveValues() {
    for (var i = 0; i < 2; ++i)
        setTimeout(() => { throw "Primitive value"; }, 0);
}
//# sourceURL=console-repeat-count.html
</script>

<script>
async function test()
{
    await InspectorTest.evaluateInPagePromise("dumpMessages()");
    await InspectorTest.evaluateInPagePromise("throwPrimitiveValues()");
    await InspectorTest.evaluateInPagePromise("throwObjects()");
    InspectorTest.waitForConsoleMessages(7, () => {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    });
}
</script>
</head>
<body onload="runTest()">
<p>
Tests that repeat count is properly updated.
</p>
</body>
</html>
