// This goes before everything else to keep console message line number invariant.
var lastXHRIndex = 0;
function xhrLoadedCallback()
{
    // We need to make sure the console message text is unique so that we don't end up with repeat count update only.
    console.log("XHR loaded: " + (++lastXHRIndex));
}

function makeSimpleXHR(method, url, async, callback)
{
    makeSimpleXHRWithPayload(method, url, async, null, callback);
}

function makeSimpleXHRWithPayload(method, url, async, payload, callback)
{
    makeXHR(method, url, async, undefined, undefined, [], false, payload, callback)
}

function makeXHR(method, url, async, user, password, headers, withCredentials, payload, type, callback)
{
    var xhr = new XMLHttpRequest();
    if (type == undefined)
        xhr.responseType = "";
    else
        xhr.responseType = type;
    xhr.onreadystatechange = function()
    {
        if (xhr.readyState === XMLHttpRequest.DONE) {
            if (typeof(callback) === "function")
                callback();
        }
    }
    xhr.open(method, url, async, user, password);
    xhr.withCredentials = withCredentials;
    for (var  i = 0; i < headers.length; ++i)
        xhr.setRequestHeader(headers[i][0], headers[i][1]);
    xhr.send(payload);
}

function makeXHRForJSONArguments(jsonArgs)
{
    var args = JSON.parse(jsonArgs);
    makeXHR(args.method, args.url, args.async, args.user, args.password, args.headers || [], args.withCredentials, args.payload, args.type, xhrLoadedCallback);
}

function makeFetch(url, requestInitializer)
{
    return fetch(url, requestInitializer).then(res => {
        // Call text(). Otherwise the backpressure mechanism may block loading.
        res.text();
        return res;
    }).catch(e => e);
}

var initialize_NetworkTest = function() {

InspectorTest.preloadPanel("network");

/**
 * @param {!SDK.NetworkRequest} request
 * @return {!Promise<!SDK.NetworkRequest>}
 */
InspectorTest.waitForRequestResponse = function(request)
{
    if (request.responseReceivedTime !== -1)
        return Promise.resolve(request);
    return InspectorTest.waitForEvent(SDK.NetworkManager.Events.RequestUpdated, InspectorTest.networkManager,
        updateRequest => updateRequest === request && request.responseReceivedTime !== -1);
}

/**
 * @param {!SDK.NetworkRequest} request
 * @return {!Promise<!Network.NetworkRequestNode>}
 */
InspectorTest.waitForNetworkLogViewNodeForRequest = function(request)
{
    var networkLogView = UI.panels.network._networkLogView;
    var node = networkLogView.nodeForRequest(request);
    if (node)
        return Promise.resolve(node);

    console.assert(networkLogView._staleRequests.has(request));

    return InspectorTest.addSnifferPromise(networkLogView, '_didRefreshForTest').then(() => {
        var node = networkLogView.nodeForRequest(request);
        console.assert(node);
        return node;
    });
}

/**
 * @param {!SDK.NetworkRequest} wsRequest
 * @param {string} message
 * @return {!Promise<!SDK.NetworkRequest.WebSocketFrame>}
 */
InspectorTest.waitForWebsocketFrameReceived = function(wsRequest, message)
{
    for (var frame of wsRequest.frames()) {
        if (checkFrame(frame))
            return Promise.resolve(frame);
    }
    return InspectorTest.waitForEvent(SDK.NetworkRequest.Events.WebsocketFrameAdded, wsRequest, checkFrame);

    function checkFrame(frame) {
        return frame.type === SDK.NetworkRequest.WebSocketFrameType.Receive && frame.text === message;
    }
}

InspectorTest.recordNetwork = function()
{
    UI.panels.network._networkLogView.setRecording(true);
}

InspectorTest.networkRequests = function()
{
    return Array.from(NetworkLog.networkLog.requests());
}

InspectorTest.dumpNetworkRequests = function()
{
    var requests = InspectorTest.networkRequests();
    requests.sort(function(a, b) {return a.url().localeCompare(b.url());});
    InspectorTest.addResult("resources count = " + requests.length);
    for (i = 0; i < requests.length; i++)
        InspectorTest.addResult(requests[i].url());
}

// |url| must be a regular expression to match request URLs.
InspectorTest.findRequestsByURLPattern = function(urlPattern)
{
    return InspectorTest.networkRequests().filter(function(value) {
        return urlPattern.test(value.url());
    });
}

InspectorTest.makeSimpleXHR = function(method, url, async, callback)
{
    InspectorTest.makeXHR(method, url, async, undefined, undefined, [], false, undefined, undefined, callback);
}

InspectorTest.makeSimpleXHRWithPayload = function(method, url, async, payload, callback)
{
    InspectorTest.makeXHR(method, url, async, undefined, undefined, [], false, payload, undefined, callback);
}

InspectorTest.makeXHR = function(method, url, async, user, password, headers, withCredentials, payload, type, callback)
{
    var args = {};
    args.method = method;
    args.url = url;
    args.async = async;
    args.user = user;
    args.password = password;
    args.headers = headers;
    args.withCredentials = withCredentials;
    args.payload = payload;
    args.type = type;
    var jsonArgs = JSON.stringify(args).replace(/\"/g, "\\\"");

    function innerCallback(msg)
    {
        if (msg.messageText.indexOf("XHR loaded") !== -1) {
            if (callback)
                callback();
        } else {
            InspectorTest.addConsoleSniffer(innerCallback);
        }
    }

    InspectorTest.addConsoleSniffer(innerCallback);
    InspectorTest.evaluateInPage("makeXHRForJSONArguments(\"" + jsonArgs + "\")");
}

InspectorTest.makeFetch = function(url, requestInitializer, callback)
{
    InspectorTest.callFunctionInPageAsync("makeFetch", [url, requestInitializer]).then(callback);
}

/**
 * @return {!Promise}
 */
InspectorTest.clearNetworkCache = function()
{
    // This turns cache off and then on, effectively clearning the cache.
    var networkAgent = InspectorTest.NetworkAgent;
    var promise = networkAgent.setCacheDisabled(true);
    return promise.then(() => networkAgent.setCacheDisabled(false));
}

InspectorTest.HARPropertyFormatters = {
    bodySize: "formatAsTypeName",
    compression: "formatAsTypeName",
    connection: "formatAsTypeName",
    headers: "formatAsTypeName",
    headersSize: "formatAsTypeName",
    id: "formatAsTypeName",
    onContentLoad: "formatAsTypeName",
    onLoad: "formatAsTypeName",
    receive: "formatAsTypeName",
    startedDateTime: "formatAsRecentTime",
    time: "formatAsTypeName",
    timings: "formatAsTypeName",
    version: "formatAsTypeName",
    wait: "formatAsTypeName",
    _transferSize: "formatAsTypeName",
    _error: "skip"
};
// addObject checks own properties only, so make a deep copy rather than use prototype.
InspectorTest.HARPropertyFormattersWithSize = JSON.parse(JSON.stringify(InspectorTest.HARPropertyFormatters));
InspectorTest.HARPropertyFormattersWithSize.size = "formatAsTypeName";

};
