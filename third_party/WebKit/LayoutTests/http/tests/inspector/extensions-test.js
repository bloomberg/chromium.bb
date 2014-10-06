function extensionFunctions()
{
    var functions = "";

    for (symbol in window) {
        if (/^extension_/.exec(symbol) && typeof window[symbol] === "function")
            functions += window[symbol].toString();
    }
    return functions;
}

var initialize_ExtensionsTest = function()
{

WebInspector.extensionServerProxy._overridePlatformExtensionAPIForTest = function(extensionInfo)
{
    WebInspector.extensionServerProxy._extensionServer._registerHandler("evaluateForTestInFrontEnd", onEvaluate);

    function platformExtensionAPI(coreAPI)
    {
        window.webInspector = coreAPI;
        window._extensionServerForTests = extensionServer;
    }
    return platformExtensionAPI.toString();
}

InspectorTest._replyToExtension = function(requestId, port)
{
    WebInspector.extensionServer._dispatchCallback(requestId, port);
}

function onEvaluate(message, port)
{
    function reply(param)
    {
        WebInspector.extensionServerProxy._extensionServer._dispatchCallback(message.requestId, port, param);
    }

    try {
        eval(message.expression);
    } catch (e) {
        InspectorTest.addResult("Exception while running: " + message.expression + "\n" + (e.stack || e));
        InspectorTest.completeTest();
    }
}

InspectorTest.showPanel = function(panelId)
{
    if (panelId === "extension")
        panelId = WebInspector.inspectorView._tabbedPane._tabs[WebInspector.inspectorView._tabbedPane._tabs.length - 1].id;
    return WebInspector.inspectorView.showPanel(panelId);
}

InspectorTest.runExtensionTests = function()
{
    WebInspector.extensionServerProxy.setFrontendReady();
    RuntimeAgent.evaluate("location.href", "console", false, function(error, result) {
        if (error)
            return;
        var pageURL = result.value;
        var extensionURL = (/^https?:/.test(pageURL) ?
            pageURL.replace(/^(https?:\/\/[^/]*\/).*$/,"$1") :
            pageURL.replace(/\/inspector\/extensions\/[^/]*$/, "/http/tests")) +
            "/inspector/resources/extension-main.html";
        WebInspector.addExtensions([{ startPage: extensionURL, name: "test extension", exposeWebInspectorNamespace: true }]);
    });
}

}

function extension_showPanel(panelId, callback)
{
    evaluateOnFrontend("InspectorTest.showPanel(unescape('" + escape(panelId) + "')).then(function() { reply(); });", callback);
}

var test = function()
{
    InspectorTest.runExtensionTests();
}
