var initialize_InterceptionTest = function() {

var interceptionRequestParams = {};
var requestIdToFilename = {};
var filenameToInterceptionId = {};
var loggedMessages = {};
var InterceptionIdToCanonicalInterceptionId = {};
var idToCanoncalId = {};
var nextId = 1;

function getNextId()
{
    return "ID " + nextId++;
}

function canonicalId(id)
{
    if (!idToCanoncalId.hasOwnProperty(id))
        idToCanoncalId[id] = getNextId();
    return idToCanoncalId[id];
}

function log(id, message)
{
    testRunner.logToStderr("id " + id + " " + message);
    if (!loggedMessages.hasOwnProperty(id))
        loggedMessages[id] = [];
    loggedMessages[id].push(message);
}

function completeTest(message)
{
    // The order in which network events occur is not fully deterministic so we
    // sort based on the interception ID to try and make the test non-flaky.
    for (var property in loggedMessages) {
        if (loggedMessages.hasOwnProperty(property)) {
            var messages = loggedMessages[property];
            for (var i = 0; i < messages.length; i++) {
                InspectorTest.log(messages[i]);
            }
        }
    }

    if (message)
        InspectorTest.log(message);

    InspectorTest.completeTest();
}

InspectorTest.startInterceptionTest = function(requestInterceptedDict,
                                               numConsoleLogsToWaitFor) {
    if (typeof numConsoleLogsToWaitFor === "undefined")
        numConsoleLogsToWaitFor = 0;

    InspectorTest.eventHandler["Network.requestIntercepted"] = onRequestIntercepted;
    InspectorTest.eventHandler["Network.loadingFailed"] = onLoadingFailed;
    InspectorTest.eventHandler["Network.requestWillBeSent"] = onRequestWillBeSent;
    InspectorTest.eventHandler["Network.responseReceived"] = onResponseReceived;
    InspectorTest.eventHandler["Runtime.consoleAPICalled"] = onConsoleAPICalled;
    InspectorTest.eventHandler["Page.frameStoppedLoading"] = onStop;

    var frameStoppedLoading = false;

    function getInterceptionId(filename) {
        if (!filenameToInterceptionId.hasOwnProperty(filename)) {
            filenameToInterceptionId[filename] = getNextId()
        }
        return filenameToInterceptionId[filename];
    }

    function enableNetwork()
    {
        InspectorTest.log("Test started");
        InspectorTest.sendCommand("Network.enable", {}, didEnableNetwork);
    }

    function didEnableNetwork(messageObject)
    {
        if (messageObject.error) {
            completeTest("FAIL: Couldn't enable network agent" +
                                  messageObject.error.message);
            return;
        }
        InspectorTest.log("Network agent enabled");
        InspectorTest.sendCommand(
            "Network.enableRequestInterception", {"enabled": true},
            didEnableRequestInterception);
    }

    function didEnableRequestInterception(messageObject)
    {
        if (messageObject.error) {
            completeTest("FAIL: Couldn't enable fetch interception" +
                                    messageObject.error.message);
            return;
        }
        InspectorTest.log("Request interception enabled");
        InspectorTest.sendCommand("Page.enable", {}, didEnablePage);
    }

    function didEnablePage(messageObject)
    {
        if (messageObject.error) {
            completeTest("FAIL: Couldn't enable page agent" +
                                    messageObject.error.message);
            return;
        }
        InspectorTest.log("Page agent enabled");

        InspectorTest.sendCommand("Runtime.enable", {}, didEnableRuntime);
    }

    function didEnableRuntime(messageObject)
    {
        if (messageObject.error) {
            completeTest("FAIL: Couldn't enable runtime agent" +
                                    messageObject.error.message);
            return;
        }
        InspectorTest.log("Runtime agent enabled");

        InspectorTest.sendCommand(
            "Runtime.evaluate", { "expression": "appendIframe()"});
    }

    function onRequestIntercepted(event)
    {
        var filename = event.params.request.url.split('/').pop();
        var id = canonicalId(event.params.interceptionId);
        filenameToInterceptionId[filename] = id;
        if (!requestInterceptedDict.hasOwnProperty(filename)) {
            completeTest("FAILED: unexpected request interception " +
                             JSON.stringify(event.params));
            return;
        }
        if (event.params.hasOwnProperty("authChallenge")) {
            log(id, "Auth required for " + id);
            requestInterceptedDict[filename + '+Auth'](event);
            return;
        } else if (event.params.hasOwnProperty("redirectUrl")) {
            log(id, "Network.requestIntercepted " + id + " " +
                    event.params.redirectStatusCode + " redirect " +
                    interceptionRequestParams[id].url.split('/').pop() +
                    " -> " + event.params.redirectUrl.split('/').pop());
            interceptionRequestParams[id].url = event.params.redirectUrl;
        } else {
            interceptionRequestParams[id] = event.params.request;
            log(id, "Network.requestIntercepted " + id + " " +
                    event.params.request.method + " " + filename + " type: " +
                    event.params.resourceType);
        }
        requestInterceptedDict[filename](event);
    }

    function onLoadingFailed(event)
    {
        var filename = requestIdToFilename[event.params.requestId];
        var id = getInterceptionId(filename);
        log(id, "Network.loadingFailed " + filename + " " +
                event.params.errorText);
    }

    function onRequestWillBeSent(event)
    {
        var filename = event.params.request.url.split('/').pop();
        requestIdToFilename[event.params.requestId] = filename;
    }

    function onResponseReceived(event)
    {
        var response = event.params.response;
        var filename = response.url.split('/').pop();
        var id = getInterceptionId(filename);
        log(id, "Network.responseReceived " + filename + " " + response.status +
                " " + response.mimeType);
    }

    function onStop()
    {
        frameStoppedLoading = true;
        log(getNextId(), "Page.frameStoppedLoading");

        maybeCompleteTest();
    }

    function onConsoleAPICalled(messageObject)
    {
        if (messageObject.params.type !== "log")
            return;

        numConsoleLogsToWaitFor--;
        maybeCompleteTest();
    }

    // Wait until we've seen Page.frameStoppedLoading and the expected number of
    // console logs.
    function maybeCompleteTest() {
        if (numConsoleLogsToWaitFor === 0 && frameStoppedLoading)
            completeTest();
    }

    enableNetwork();
}

InspectorTest.allowRequest = function(event) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "allowRequest " + id);
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId
    });
}

InspectorTest.modifyRequest = function(event, params) {
    var id = canonicalId(event.params.interceptionId);
    var mods = [];
    for (property in params) {
        if (!params.hasOwnProperty(property))
            continue;
        if (property === "url") {
            var newUrl = params["url"];
            var filename = interceptionRequestParams[id].url;
            mods.push("url " + filename.split('/').pop() + " -> " + newUrl);
            var directoryPath =
                filename.substring(0, filename.lastIndexOf('/') + 1);
            params["url"] = directoryPath + newUrl;
        } else {
            mods.push(property + " " +
                      JSON.stringify(interceptionRequestParams[id][property]) +
                      " -> " + JSON.stringify(params[property]));
        }
    }

    log(id, "modifyRequest " + id + ": " + mods.join("; "));
    params["interceptionId"] = event.params.interceptionId;
    InspectorTest.sendCommand("Network.continueInterceptedRequest", params);
}

InspectorTest.blockRequest = function(event, errorReason) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "blockRequest " + id + " " + errorReason);
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId,
        "errorReason": errorReason
    });
}

InspectorTest.mockResponse = function(event, rawResponse) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "mockResponse " + id);
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId,
        "rawResponse": btoa(rawResponse)
    });
}

InspectorTest.disableRequestInterception = function(event) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "----- disableRequestInterception -----");
    InspectorTest.sendCommand("Network.enableRequestInterception", {
        "enabled": false,
    });
}

InspectorTest.cancelAuth = function(event) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "----- Cancel Auth -----");
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId,
        "authChallengeResponse": {"response": "CancelAuth"}
    });
}

InspectorTest.defaultAuth = function(event) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "----- Use Default Auth -----");
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId,
        "authChallengeResponse": {"response": "Default"}
    });
}

InspectorTest.provideAuthCredentials = function(event, username, password) {
    var id = canonicalId(event.params.interceptionId);
    log(id, "----- Provide Auth Credentials -----");
    InspectorTest.sendCommand("Network.continueInterceptedRequest", {
        "interceptionId": event.params.interceptionId,
        "authChallengeResponse": {
            "response": "ProvideCredentials",
            "username": username,
            "password": password
        }
    });
}

}
