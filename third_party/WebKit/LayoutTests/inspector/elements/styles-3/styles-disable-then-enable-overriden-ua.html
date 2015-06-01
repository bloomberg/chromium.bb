<!DOCTYPE html>
<html>
<head>
<script src="../../../http/tests/inspector/inspector-test.js"></script>
<script src="../../../http/tests/inspector/elements-test.js"></script>

<script>

function test()
{
    InspectorTest.selectNodeAndWaitForStyles("body-id", step1);

    function step1()
    {
        InspectorTest.addResult("Before disable");
        InspectorTest.dumpSelectedElementStyles(true, false, true);
        InspectorTest.toggleStyleProperty("margin", false);
        InspectorTest.waitForStyles("body-id", step2);
    }

    function step2()
    {
        InspectorTest.addResult("After disable");
        InspectorTest.dumpSelectedElementStyles(true, false, true);
        InspectorTest.toggleStyleProperty("margin", true);
        InspectorTest.waitForStyles("body-id", step3);
    }

    function step3()
    {
        InspectorTest.addResult("After enable");
        InspectorTest.dumpSelectedElementStyles(true, false, true);
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="runTest()" id="body-id" style="margin: 10px">
<p>
Tests that disabling shorthand removes the "overriden" mark from the UA shorthand it overrides.
</p>

</body>
</html>
