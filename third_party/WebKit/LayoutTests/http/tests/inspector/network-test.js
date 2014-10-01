// This goes before everything else to keep console message line number invariant.
var lastXHRIndex = 0;
function xhrLoadedCallback()
{
    // We need to make sure the console message text is unique so that we don't end up with repeat count update only.
    console.log("XHR loaded: " + (++lastXHRIndex));
}

var initialize_NetworkTest = function() {

InspectorTest.preloadPanel("network");

InspectorTest.recordNetwork = function()
{
    WebInspector.panels.network._networkLogView._recordButton.toggled = true;
}

InspectorTest.networkRequests = function()
{
    return WebInspector.networkLog.requests.slice();
}

InspectorTest.dumpNetworkRequests = function()
{
    var requests = InspectorTest.networkRequests();
    requests.sort(function(a, b) {return a.url.localeCompare(b.url);});
    InspectorTest.addResult("resources count = " + requests.length);
    for (i = 0; i < requests.length; i++)
        InspectorTest.addResult(requests[i].url);
}

InspectorTest.resetInspectorResourcesData = function(callback)
{
    InspectorTest.evaluateInPage("resetInspectorResourcesData()", nextStep);

    function nextStep(result)
    {
        if (!result) {
            InspectorTest.addResult("This test can not be run as window.internals is not available.");
            Inspector.completeTest();
        } else
            callback();
    }
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
        if (msg.messageText.indexOf("XHR loaded") !== -1)
            callback();
        else
            InspectorTest.addConsoleSniffer(innerCallback);
    }

    InspectorTest.addConsoleSniffer(innerCallback);
    InspectorTest.evaluateInPage("makeXHRForJSONArguments(\"" + jsonArgs + "\")");
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
    _error: "skip",
};

// addObject checks own properties only, so make a deep copy rather than use prototype.

InspectorTest.HARPropertyFormattersWithSize = JSON.parse(JSON.stringify(InspectorTest.HARPropertyFormatters));
InspectorTest.HARPropertyFormattersWithSize.size = "formatAsTypeName";

};

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


function resetInspectorResourcesData()
{
    if (!window.internals)
        return false;

    internals.setInspectorResourcesDataSizeLimits(10 * 1000 * 1000, 1000 * 1000);
    return true;
}

