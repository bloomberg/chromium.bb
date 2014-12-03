var initialize_SourcesTest = function() {

InspectorTest.preloadPanel("sources");

function testSourceMapping(text1, text2, mapping, testToken)
{
    var originalPosition = text1.indexOf(testToken);
    InspectorTest.assertTrue(originalPosition !== -1);
    var originalLocation = WebInspector.Formatter.positionToLocation(text1.lineEndings(), originalPosition);
    var formattedLocation = mapping.originalToFormatted(originalLocation[0], originalLocation[1]);
    var formattedPosition = WebInspector.Formatter.locationToPosition(text2.lineEndings(), formattedLocation[0], formattedLocation[1]);
    var expectedFormattedPosition = text2.indexOf(testToken);
    if (expectedFormattedPosition === formattedPosition)
        InspectorTest.addResult(String.sprintf("Correct mapping for <%s>", testToken));
    else
        InspectorTest.addResult(String.sprintf("ERROR: Wrong mapping for <%s>", testToken));
};

InspectorTest.testPrettyPrint = function(mimeType, text, mappingQueries, next)
{
    new WebInspector.ScriptFormatter(mimeType, text, didFormatContent);

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

};
