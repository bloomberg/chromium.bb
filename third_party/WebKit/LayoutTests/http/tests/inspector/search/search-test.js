// To ensure that no GC-able resources from other tests upset
// expectations, perform an initial GC.
if (window.GCController)
    GCController.collect();

var initialize_SearchTest = function() {
InspectorTest.dumpSearchResults = function(searchResults)
{
    function comparator(a, b)
    {
        a.url.localeCompare(b.url);
    }
    searchResults.sort(comparator);

    InspectorTest.addResult("Search results: ");
    for (var i = 0; i < searchResults.length; i++)
        InspectorTest.addResult("url: " + searchResults[i].url.replace(/VM\d+/, "VMXX") + ", matchesCount: " + searchResults[i].matchesCount);
    InspectorTest.addResult("");
};

InspectorTest.dumpSearchMatches = function(searchMatches)
{
    InspectorTest.addResult("Search matches: ");
    for (var i = 0; i < searchMatches.length; i++)
        InspectorTest.addResult("lineNumber: " + searchMatches[i].lineNumber + ", line: '" + searchMatches[i].lineContent + "'");
    InspectorTest.addResult("");
};

InspectorTest.runSearchAndDumpResults = function(scope, searchConfig, callback)
{
    var searchResults = [];
    var progress = new Common.Progress();
    scope.performSearch(searchConfig, progress, searchResultCallback, searchFinishedCallback);

    function searchResultCallback(searchResult)
    {
        searchResults.push(searchResult);
    }

    function searchFinishedCallback()
    {
        function comparator(searchResultA, searchResultB)
        {
            return searchResultA.uiSourceCode.url().compareTo(searchResultB.uiSourceCode.url());
        }
        searchResults.sort(comparator);

        for (var i = 0; i < searchResults.length; ++i) {
            var searchResult = searchResults[i];
            var uiSourceCode = searchResult.uiSourceCode;
            var searchMatches = searchResult.searchMatches;

            if (!searchMatches.length)
                continue;
            InspectorTest.addResult("Search result #" + (i + 1) + ": uiSourceCode.url = " + uiSourceCode.url().replace(/VM\d+/, "VMXX"));
            for (var j = 0; j < searchMatches.length; ++j) {
                var lineNumber = searchMatches[j].lineNumber;
                var lineContent = searchMatches[j].lineContent;
                InspectorTest.addResult("  search match #" + (j + 1) + ": lineNumber = " + lineNumber + ", lineContent = '" + lineContent + "'");
            }
        }
        callback();
    }
}

InspectorTest.replaceAndDumpChange = function(sourceFrame, searchConfig, replacement, replaceAll)
{
    var modifiers = [];
    if (searchConfig.isRegex)
        modifiers.push("regex");
    if (searchConfig.caseSensitive)
        modifiers.push("caseSensitive");
    if (replaceAll)
        modifiers.push("replaceAll");
    var modifiersString = modifiers.length ?  " (" + modifiers.join(", ") + ")" : "";

    InspectorTest.addResult("Running replace test for /" + searchConfig.query + "/" + replacement + "/ " + modifiersString + ":");

    editor = sourceFrame._textEditor;
    var oldLines = [];
    for (var i = 0; i < editor.linesCount; ++i)
        oldLines.push(editor.line(i));
    var searchableView = UI.panels.sources.sourcesView().searchableView();
    searchableView.showSearchField();

    searchableView._caseSensitiveButton.setToggled(searchConfig.caseSensitive);
    searchableView._regexButton.setToggled(searchConfig.isRegex);
    searchableView._searchInputElement.value = searchConfig.query;
    searchableView._replaceCheckboxElement.checked = true;
    searchableView._updateSecondRowVisibility();
    searchableView._replaceInputElement.value = replacement;
    searchableView._performSearch(true, true);
    if (replaceAll)
        searchableView._replaceAll()
     else
        searchableView._replace();

    var newLines = [];
    for (var i = 0; i < editor.linesCount; ++i)
        newLines.push(editor.line(i));

    for (var i = 0; i < newLines.length; ++i) {
        if (oldLines[i] === newLines[i])
            continue;
        var oldLine = oldLines[i];
        var newLine = newLines[i];

        var prefixLength = 0;
        for (var j = 0; j < oldLine.length && j < newLine.length && newLine[j] === oldLine[j]; ++j)
            ++prefixLength;
        var postfixLength = 0;
        for (var j = 0; j < oldLine.length && j < newLine.length && newLine[newLine.length - j - 1] === oldLine[oldLine.length - j - 1]; ++j)
            ++postfixLength;
        var prefix = oldLine.substring(0, prefixLength);
        var removed = oldLine.substring(prefixLength, oldLine.length - postfixLength);
        var added = newLine.substring(prefixLength, newLine.length - postfixLength);
        var postfix = oldLine.substring(oldLine.length - postfixLength);
        InspectorTest.addResult("  - " + prefix + "#" + removed + "#" + added + "#" + postfix);
    }
}

};
