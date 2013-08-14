var initialize_CanvasWebGLProfilerTest = function() {

InspectorTest.enableCanvasAgent = function(callback)
{
    var dispatcher = InspectorBackend._domainDispatchers["Canvas"];
    if (!dispatcher) {
        InspectorBackend.registerCanvasDispatcher({
            contextCreated: function() {},
            traceLogsRemoved: function() {}
        });
    }

    function canvasAgentEnabled(error)
    {
        if (!error)
            InspectorTest.safeWrap(callback)();
        else {
            InspectorTest.addResult("FAILED to enable CanvasAgent: " + error);
            InspectorTest.completeTest();
        }
    }
    try {
        CanvasAgent.enable(canvasAgentEnabled);
    } catch (e) {
        InspectorTest.addResult("Exception while enabling CanvasAgent: " + e);
        InspectorTest.completeTest();
    }
};

InspectorTest.disableCanvasAgent = function(callback)
{
    function canvasAgentDisabled(error)
    {
        if (!error)
            InspectorTest.safeWrap(callback)();
        else {
            InspectorTest.addResult("FAILED to disable CanvasAgent: " + error);
            InspectorTest.completeTest();
        }
    }
    try {
        CanvasAgent.disable(canvasAgentDisabled);
    } catch (e) {
        InspectorTest.addResult("Exception while disabling CanvasAgent: " + e);
        InspectorTest.completeTest();
    }
};

InspectorTest.callArgumentDescription = function(callArg)
{
    return callArg.enumName || callArg.description;
};

InspectorTest.dumpTraceLogCall = function(call, indent, filter)
{
    indent = indent || "";
    function show(name)
    {
        return !filter || filter[name];
    }
    function formatSourceURL(url)
    {
        return url ? "\"" + url.replace(/^.*\/([^\/]+)\/?$/, "$1") + "\"" : "null";
    }
    var args = (call.arguments || []).map(InspectorTest.callArgumentDescription);
    var properties = [
        "{Call}",
        (show("functionName") && call.functionName) ? "functionName:\"" + call.functionName + "\"" : "",
        (show("arguments") && call.arguments) ? "arguments:[" + args.join(",") + "]" : "",
        (show("result") && call.result) ? "result:" + InspectorTest.callArgumentDescription(call.result) : "",
        (show("property") && call.property) ? "property:\"" + call.property + "\"" : "",
        (show("value") && call.value) ? "value:" + InspectorTest.callArgumentDescription(call.value) : "",
        (show("isDrawingCall") && call.isDrawingCall) ? "isDrawingCall:true" : "",
        (show("isFrameEndCall") && call.isFrameEndCall) ? "isFrameEndCall:true" : "",
        show("sourceURL") ? "sourceURL:" + formatSourceURL(call.sourceURL) : "",
        show("lineNumber") ? "lineNumber:" + call.lineNumber : "",
        show("columnNumber") ? "columnNumber:" + call.columnNumber : ""
    ];
    InspectorTest.addResult(indent + properties.filter(Boolean).join("  "));
};

InspectorTest.dumpTraceLog = function(traceLog, indent, filter)
{
    indent = indent || "";
    function show(name)
    {
        return !filter || filter[name];
    }
    var calls = traceLog.calls;
    var properties = [
        "{TraceLog}",
        show("alive") ? "alive:" + !!traceLog.alive : "",
        show("startOffset") ? "startOffset:" + (traceLog.startOffset || 0) : "",
        show("totalAvailableCalls") ? "#calls:" + traceLog.totalAvailableCalls : ""
    ];
    InspectorTest.addResult(indent + properties.filter(Boolean).join("  "));
    for (var i = 0, n = calls.length; i < n; ++i)
        InspectorTest.dumpTraceLogCall(calls[i], indent + "  ", filter);
};

InspectorTest.sortResourceStateDescriptors = function(descriptors)
{
    var wordNumRegex = /^(\D+)(\d+)$/;
    function comparator(a, b)
    {
        a = a.name.replace(wordNumRegex, "$2");
        b = b.name.replace(wordNumRegex, "$2");
        if (!isNaN(a) && !isNaN(b))
            return Number(a) - Number(b);
        if (a < b)
            return -1;
        if (a > b)
            return 1;
        return 0;
    }
    descriptors.sort(comparator);
};

InspectorTest.dumpResourceStateDescriptor = function(descriptor, indent)
{
    indent = indent || "";
    if (descriptor.values) {
        var name = descriptor.name;
        if (descriptor.isArray)
            name += "[" + descriptor.values.length + "]";
        InspectorTest.addResult(indent + name);
        var descriptors = descriptor.values;
        InspectorTest.sortResourceStateDescriptors(descriptors);
        for (var i = 0, n = descriptors.length; i < n; ++i)
            InspectorTest.dumpResourceStateDescriptor(descriptors[i], indent + "  ");
    } else
        InspectorTest.addResult(indent + descriptor.name + ": " + InspectorTest.callArgumentDescription(descriptor.value));
};

InspectorTest.dumpResourceState = function(resourceState, indent)
{
    indent = indent || "";
    var descriptors = resourceState.descriptors || [];
    var properties = [
        "{ResourceState}",
        "length(imageURL):" + (resourceState.imageURL || "").length
    ];
    InspectorTest.addResult(indent + properties.filter(Boolean).join("  "));
    InspectorTest.sortResourceStateDescriptors(descriptors);
    for (var i = 0, n = descriptors.length; i < n; ++i)
        InspectorTest.dumpResourceStateDescriptor(descriptors[i], indent + "  ");
};

};

function createWebGLContext(opt_canvas)
{
    if (window.testRunner)
        testRunner.overridePreference("WebKitWebGLEnabled", "1");

    var canvas = opt_canvas || document.createElement("canvas");
    var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
    for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
        var gl = canvas.getContext(contextId);
        if (gl)
            return gl;
    }
    return null;
}

function createCanvas2DContext(opt_canvas)
{
    var canvas = opt_canvas || document.createElement("canvas");
    return canvas.getContext("2d");
}
