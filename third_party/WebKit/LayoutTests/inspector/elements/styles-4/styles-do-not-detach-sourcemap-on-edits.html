<html>
<head>

<link rel="stylesheet" href="./resources/styles-do-not-detach-sourcemap-on-edits.css">
<script src="../../../http/tests/inspector/debugger-test.js"></script>
<script src="../../../http/tests/inspector/elements-test.js"></script>
<script src="../../../http/tests/inspector/inspector-test.js"></script>
<script>

function test()
{
    InspectorTest.waitForScriptSource("styles-do-not-detach-sourcemap-on-edits.scss", onSourceMapLoaded);

    function onSourceMapLoaded()
    {
        InspectorTest.selectNodeAndWaitForStyles("container", onNodeSelected);
    }

    function onNodeSelected()
    {
        InspectorTest.runTestSuite(testSuite);
    }

    var testSuite = [
        function editProperty(next)
        {
            InspectorTest.dumpSelectedElementStyles(true, false, true);

            var treeItem = InspectorTest.getMatchedStylePropertyTreeItem("color");
            treeItem.applyStyleText("NAME: VALUE", true);
            InspectorTest.waitForStyles("container", next);
        },

        function editSelector(next)
        {
            InspectorTest.dumpSelectedElementStyles(true, false, true);

            var section = InspectorTest.firstMatchedStyleSection();
            section.startEditingSelector();
            section._selectorElement.textContent = "#container, SELECTOR";
            InspectorTest.waitForSelectorCommitted(next);
            section._selectorElement.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
        },

        function editMedia(next)
        {
            InspectorTest.dumpSelectedElementStyles(true, false, true);

            var section = InspectorTest.firstMatchedStyleSection();
            var mediaTextElement = InspectorTest.firstMediaTextElementInSection(section);
            mediaTextElement.click();
            mediaTextElement.textContent = "(max-width: 9999999px)";
            InspectorTest.waitForMediaTextCommitted(next);
            mediaTextElement.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
        },

        function addRule(next)
        {
            InspectorTest.dumpSelectedElementStyles(true, false, true);

            var styleSheetHeader = InspectorTest.cssModel.styleSheetHeaders().find(header => header.resourceURL().indexOf("styles-do-not-detach-sourcemap-on-edits.css") !== -1);
            if (!styleSheetHeader) {
                InspectorTest.addResult("ERROR: failed to find style sheet!");
                InspectorTest.completeTest();
                return;
            }
            InspectorTest.addNewRuleInStyleSheet(styleSheetHeader, "NEW-RULE", next);
        },

        function finish(next)
        {
            InspectorTest.dumpSelectedElementStyles(true, false, true);
            next();
        },
    ];
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that source map is not detached on edits. crbug.com/257778
</p>

<div id="container">container.</div>

</body>
</html>
