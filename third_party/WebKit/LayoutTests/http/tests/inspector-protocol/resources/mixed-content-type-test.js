if (window.testRunner) {
    testRunner.overridePreference("WebKitAllowRunningInsecureContent", true);
}

function test()
{
    InspectorTest.eventHandler["Network.requestWillBeSent"] = onRequestWillBeSent;
    InspectorTest.sendCommand("Network.enable", {}, didEnableNetwork);

    function didEnableNetwork(messageObject)
    {
        if (messageObject.error) {
            InspectorTest.log("FAIL: Couldn't enable network agent" + messageObject.error.message);
            InspectorTest.completeTest();
            return;
        }
        InspectorTest.log("Network agent enabled");
        InspectorTest.sendCommand("Runtime.evaluate", { "expression": "addIframeWithMixedContent()" });
    }

    var numRequests = 0;
    function onRequestWillBeSent(event) {
        var req = event.params.request;
        InspectorTest.log("Mixed content type of " + req.url + ": " + req.mixedContentType);
        numRequests++;
        if (numRequests == 2)
            InspectorTest.completeTest();
    }
}
