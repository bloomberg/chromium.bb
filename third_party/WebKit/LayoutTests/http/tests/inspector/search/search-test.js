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
        InspectorTest.addResult("url: " + searchResults[i].url + ", matchesCount: " + searchResults[i].matchesCount);
    InspectorTest.addResult("");
};

InspectorTest.dumpSearchMatches = function(searchMatches)
{
    InspectorTest.addResult("Search matches: ");
    for (var i = 0; i < searchMatches.length; i++)
        InspectorTest.addResult("lineNumber: " + searchMatches[i].lineNumber + ", line: '" + searchMatches[i].lineContent + "'");
    InspectorTest.addResult("");
};

InspectorTest.runSearchAndDumpResults = function(scope, searchConfig, sortByURI, callback)
{
    var searchResults = [];
    var progress = new WebInspector.Progress();
    scope.performSearch(searchConfig, progress, searchResultCallback, searchFinishedCallback);

    function searchResultCallback(searchResult)
    {
        searchResults.push(searchResult);
    }

    function searchFinishedCallback()
    {
        function comparator(searchResultA, searchResultB)
        {
            return searchResultA.uiSourceCode.uri().compareTo(searchResultB.uiSourceCode.uri());
        }
        if (sortByURI)
            searchResults.sort(comparator);

        for (var i = 0; i < searchResults.length; ++i) {
            var searchResult = searchResults[i];
            var uiSourceCode = searchResult.uiSourceCode;
            var searchMatches = searchResult.searchMatches;

            if (!searchMatches.length)
                continue;
            InspectorTest.addResult("Search result #" + (i + 1) + ": uiSourceCode.uri = " + uiSourceCode.uri());
            for (var j = 0; j < searchMatches.length; ++j) {
                var lineNumber = searchMatches[j].lineNumber;
                var lineContent = searchMatches[j].lineContent;
                InspectorTest.addResult("  search match #" + (j + 1) + ": lineNumber = " + lineNumber + ", lineContent = '" + lineContent + "'");
            }
        }
        callback();
    }
}

};
