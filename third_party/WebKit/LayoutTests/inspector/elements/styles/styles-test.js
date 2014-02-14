function initialize_StylesTests()
{

InspectorTest.waitForStylesheetsOnFrontend = function(styleSheetsCount, callback)
{
    function styleSheetComparator(a, b)
    {
        if (a.sourceURL < b.sourceURL)
            return -1;
        else if (a.sourceURL > b.sourceURL)
            return 1;
        return a.startLine - b.startLine || a.startColumn - b.startColumn;
    }

    var styleSheets = WebInspector.cssModel.allStyleSheets();
    if (styleSheets.length >= styleSheetsCount) {
        styleSheets.sort(styleSheetComparator);
        callback(styleSheets);
        return;
    }

    function onStyleSheetAdded()
    {
        var styleSheets = WebInspector.cssModel.allStyleSheets();
        if (styleSheets.length < styleSheetsCount)
            return;

        WebInspector.cssModel.removeEventListener(WebInspector.CSSStyleModel.Events.StyleSheetAdded, onStyleSheetAdded, this);
        styleSheets.sort(styleSheetComparator);
        callback(null, styleSheets);
    }

    WebInspector.cssModel.addEventListener(WebInspector.CSSStyleModel.Events.StyleSheetAdded, onStyleSheetAdded, this);
}

}
