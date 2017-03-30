<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/sources-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function test()
{
    InspectorTest.addSniffer(Workspace.UISourceCode.prototype, "addLineMessage", dumpLineMessage, true);

    InspectorTest.runTestSuite([
        function testConsoleLogAndReturnMessageLocation(next)
        {
            InspectorTest.waitUntilNthMessageReceivedPromise(2)
                .then(() => InspectorTest.dumpConsoleMessages())
                .then(() => Console.ConsoleView.clearConsole())
                .then(() => next());

            createSnippetPromise("console.log(239);42")
                .then(uiSourceCode => selectSourceCode(uiSourceCode))
                .then(uiSourceCode => renameSourceCodePromise("name1", uiSourceCode))
                .then(() => runSelectedSnippet());
        },

        function testSnippetSyntaxError(next)
        {
            InspectorTest.waitUntilNthMessageReceivedPromise(1)
                .then(() => InspectorTest.dumpConsoleMessages())
                .then(() => Console.ConsoleView.clearConsole())
                .then(() => next());

            createSnippetPromise("\n }")
                .then(uiSourceCode => selectSourceCode(uiSourceCode))
                .then(uiSourceCode => renameSourceCodePromise("name2", uiSourceCode))
                .then(() => runSelectedSnippet());
        },

        function testConsoleErrorHighlight(next)
        {
            InspectorTest.waitUntilNthMessageReceivedPromise(1)
                .then(() => InspectorTest.dumpConsoleMessages())
                .then(() => Console.ConsoleView.clearConsole())
                .then(() => next());

            createSnippetPromise("\n  console.error(42);")
                .then(uiSourceCode => selectSourceCode(uiSourceCode))
                .then(uiSourceCode => renameSourceCodePromise("name3", uiSourceCode))
                .then(() => runSelectedSnippet());
        }
    ]);

    function createSnippetPromise(content)
    {
        var callback;
        var promise = new Promise(fullfill => callback = fullfill);
        Snippets.scriptSnippetModel._project.createFile("", null, content, callback);
        return promise;
    }

    function renameSourceCodePromise(newName, uiSourceCode)
    {
        var callback;
        var promise = new Promise(fullfill => callback = fullfill);
        uiSourceCode.rename(newName).then(() => callback(uiSourceCode));
        return promise;
    }

    function selectSourceCode(uiSourceCode)
    {
        return Common.Revealer.revealPromise(uiSourceCode).then(() => uiSourceCode);
    }

    function dumpLineMessage(level, text, lineNumber, columnNumber)
    {
        InspectorTest.addResult(`Line Message was added: ${this.url()} ${level} '${text}':${lineNumber}:${columnNumber}`);
    }

    function runSelectedSnippet()
    {
        Sources.SourcesPanel.instance()._runSnippet();
    }
}
</script>
</head>
<body onload="runTest()">
<p>Test that link to snippet works.</p>
</body>
</html>
