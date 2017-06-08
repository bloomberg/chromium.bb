var initialize_SourcesTest = function() {

InspectorTest.preloadPanel("sources");

function testSourceMapping(text1, text2, mapping, testToken)
{
    var originalPosition = text1.indexOf(testToken);
    InspectorTest.assertTrue(originalPosition !== -1);
    var originalLocation = Formatter.Formatter.positionToLocation(text1.computeLineEndings(), originalPosition);
    var formattedLocation = mapping.originalToFormatted(originalLocation[0], originalLocation[1]);
    var formattedPosition = Formatter.Formatter.locationToPosition(text2.computeLineEndings(), formattedLocation[0], formattedLocation[1]);
    var expectedFormattedPosition = text2.indexOf(testToken);
    if (expectedFormattedPosition === formattedPosition)
        InspectorTest.addResult(String.sprintf("Correct mapping for <%s>", testToken));
    else
        InspectorTest.addResult(String.sprintf("ERROR: Wrong mapping for <%s>", testToken));
};

InspectorTest.testPrettyPrint = function(mimeType, text, mappingQueries, next)
{
    new Formatter.ScriptFormatter(mimeType, text, didFormatContent);

    function didFormatContent(formattedSource, mapping)
    {
        InspectorTest.addResult("====== 8< ------");
        InspectorTest.addResult(formattedSource);
        InspectorTest.addResult("------ >8 ======");
        while (mappingQueries && mappingQueries.length)
            testSourceMapping(text, formattedSource, mapping, mappingQueries.shift());

        next();
    }
}

InspectorTest.testJavascriptOutline = function(text) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    Formatter.formatterWorkerPool().javaScriptOutline(text, onChunk);
    var items = [];
    return promise;

    function onChunk(isLastChunk, outlineItems) {
        items.pushAll(outlineItems);
        if (!isLastChunk)
            return;
        InspectorTest.addResult('Text:');
        InspectorTest.addResult(text.split('\n').map(line => '    ' + line).join('\n'));
        InspectorTest.addResult('Outline:');
        for (var item of items)
            InspectorTest.addResult('    ' + item.name + (item.arguments || '') + ':' + item.line + ':' + item.column);
        fulfill();
    }
}

InspectorTest.dumpSwatchPositions = function(sourceFrame, bookmarkType)
{
    var textEditor = sourceFrame.textEditor;
    var markers = textEditor.bookmarks(textEditor.fullRange(), bookmarkType);
    for (var i = 0; i < markers.length; i++) {
        var position = markers[i].position();
        var text = markers[i]._marker.widgetNode.firstChild.textContent;
        InspectorTest.addResult("Line " + position.startLine + ", Column " + position.startColumn + ": " + text);
    }
}

};
