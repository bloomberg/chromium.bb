var initialize_ExtensionsTest = function()
{
    InspectorTest.preloadModule('extensions_test_runner');
}

// TODO(chenwilliam): these should be in ExtensionsTestRunner.js initAsync(...)
// however these are here because they need to be executed when the inspected page reloads
function extensionFunctions()
{
    var functions = "";

    for (symbol in window) {
        if (/^extension_/.exec(symbol) && typeof window[symbol] === "function")
            functions += window[symbol].toString();
    }
    return functions;
}

var extensionsOrigin = "http://devtools-extensions.oopif.test:8000";

function extension_showPanel(panelId, callback)
{
    evaluateOnFrontend("InspectorTest.showPanel(unescape('" + escape(panelId) + "')).then(function() { reply(); });", callback);
}

var test = function()
{
    Common.moduleSetting("shortcutPanelSwitch").set(true);
    ExtensionsTestRunner.runExtensionTests();
}
