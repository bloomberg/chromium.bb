<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function log()
{
    // Fill console.
    console.log("one");
    console.log("two");
    console.log("three");
}

function clearConsoleFromPage()
{
    console.clear();
}

async function test()
{
    InspectorTest.runTestSuite([
        async function clearFromConsoleAPI(next) {
            await InspectorTest.RuntimeAgent.evaluate("log();");
            InspectorTest.addResult("=== Before clear ===");
            InspectorTest.dumpConsoleMessages();

            await InspectorTest.RuntimeAgent.evaluate("clearConsoleFromPage();");

            InspectorTest.addResult("=== After clear ===");
            InspectorTest.dumpConsoleMessages();
            next();
        },

        async function shouldNotClearWithPreserveLog(next) {
            await InspectorTest.RuntimeAgent.evaluate("log();");
            InspectorTest.addResult("=== Before clear ===");
            InspectorTest.dumpConsoleMessages();
            Common.moduleSetting("preserveConsoleLog").set(true);

            await InspectorTest.RuntimeAgent.evaluate("clearConsoleFromPage();");

            InspectorTest.addResult("=== After clear ===");
            InspectorTest.dumpConsoleMessages();
            Common.moduleSetting("preserveConsoleLog").set(false);
            next();
        }
    ]);
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that console is cleared via console.clear() method
</p>
<a href="https://bugs.webkit.org/show_bug.cgi?id=101021">Bug 101021</a>

</body>
</html>
