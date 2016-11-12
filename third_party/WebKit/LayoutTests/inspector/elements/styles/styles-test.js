function initialize_StylesTests()
{

InspectorTest.waitForStylesheetsOnFrontend = function(styleSheetsCount, callback)
{
    var styleSheets = InspectorTest.cssModel.allStyleSheets();
    if (styleSheets.length >= styleSheetsCount) {
        callback(styleSheets);
        return;
    }

    function onStyleSheetAdded()
    {
        var styleSheets = InspectorTest.cssModel.allStyleSheets();
        if (styleSheets.length < styleSheetsCount)
            return;

        InspectorTest.cssModel.removeEventListener(SDK.CSSModel.Events.StyleSheetAdded, onStyleSheetAdded, this);
        styleSheets.sort(styleSheetComparator);
        callback(null, styleSheets);
    }

    InspectorTest.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, onStyleSheetAdded, this);
}

}
