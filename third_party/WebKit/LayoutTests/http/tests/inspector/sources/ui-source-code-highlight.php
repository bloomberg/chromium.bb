<html>
<head>
<script src="/inspector/inspector-test.js"></script>
<script>

function test()
{
    var uiSourceCodes = WebInspector.workspace.uiSourceCodes();

    for (var i = 0; i < uiSourceCodes.length; ++i) {
        var uiSourceCode = uiSourceCodes[i];
        if (!/.php$/.test(uiSourceCode.originURL()))
            continue;
        if (uiSourceCode.project().type() !== WebInspector.projectTypes.Network)
            continue;
        InspectorTest.addResult("Highlight mimeType: " + WebInspector.SourcesView.uiSourceCodeHighlighterType(uiSourceCode));
        InspectorTest.completeTest();
        return;
    }

    InspectorTest.addResult("Failed to find source code with .php extension.");
    InspectorTest.completeTest();
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that network-loaded UISourceCodes are highlighted according to their HTTP header mime type
instead of their extension. crbug.com/411863
</p>
</body>
</html>
