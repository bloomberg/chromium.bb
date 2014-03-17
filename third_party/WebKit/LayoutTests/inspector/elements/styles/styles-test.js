function initialize_StylesTests()
{

InspectorTest.waitForStylesheetsOnFrontend = function(styleSheetsCount, callback)
{
    var styleSheets = WebInspector.cssModel.allStyleSheets();
    if (styleSheets.length >= styleSheetsCount) {
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
