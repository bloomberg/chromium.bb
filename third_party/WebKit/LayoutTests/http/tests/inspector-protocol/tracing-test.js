// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var evalCallbackCallId = 3;

initialize_tracingHarness = function()
{

InspectorTest.startTracing = function(callback)
{
    InspectorTest.startTracingWithArguments({ "categories": "-*,disabled-by-default-devtools.timeline,devtools.timeline", "type": "", "options": "" }, callback);
}

InspectorTest.startTracingAndSaveAsStream = function(callback)
{
    var args = {
        "categories": "-*,disabled-by-default-devtools.timeline,devtools.timeline",
        "type": "",
        "options": "",
        "transferMode": "ReturnAsStream"
    };
    InspectorTest.startTracingWithArguments(args, callback);
}

InspectorTest.startTracingWithArguments = function(args, callback)
{
    InspectorTest.sendCommand("Tracing.start", args, onStart);

    function onStart(response)
    {
        InspectorTest.log("Recording started");
        callback();
    }
}

InspectorTest.stopTracing = function(callback)
{
    InspectorTest.eventHandler["Tracing.tracingComplete"] = tracingComplete;
    InspectorTest.eventHandler["Tracing.dataCollected"] = dataCollected;
    InspectorTest.sendCommand("Tracing.end", { });

    InspectorTest.devtoolsEvents = [];
    function dataCollected(reply)
    {
        var allEvents = reply.params.value;
        InspectorTest.devtoolsEvents = InspectorTest.devtoolsEvents.concat(allEvents.filter(function(e)
        {
            return /devtools.timeline/.test(e.cat);
        }));
    }

    function tracingComplete(event)
    {
        InspectorTest.log("Tracing complete");
        InspectorTest.eventHandler["Tracing.tracingComplete"] = null;
        InspectorTest.eventHandler["Tracing.dataCollected"] = null;
        callback(InspectorTest.devtoolsEvents);
    }
}

InspectorTest.stopTracingAndReturnStream = function(callback)
{
    InspectorTest.eventHandler["Tracing.tracingComplete"] = tracingComplete;
    InspectorTest.eventHandler["Tracing.dataCollected"] = dataCollected;
    InspectorTest.sendCommand("Tracing.end");

    function dataCollected(reply)
    {
        InspectorTest.log("FAIL: dataCollected event should not be fired when returning trace as stream.");

    }

    function tracingComplete(event)
    {
        InspectorTest.log("Tracing complete");
        InspectorTest.eventHandler["Tracing.tracingComplete"] = null;
        InspectorTest.eventHandler["Tracing.dataCollected"] = null;
        callback(event.params.stream);
    }
}

InspectorTest.retrieveStream = function(streamHandle, offset, chunkSize, callback)
{
    var result = "";
    var had_eof = false;

    var readArguments = { handle: streamHandle };
    if (typeof chunkSize === "number")
        readArguments.size = chunkSize;
    var firstReadArguments = JSON.parse(JSON.stringify(readArguments));
    if (typeof offset === "number")
        firstReadArguments.offset = 0;
    InspectorTest.sendCommandOrDie("IO.read", firstReadArguments, onChunkRead);
    // Assure multiple in-lfight reads are fine (also, save on latencies).
    InspectorTest.sendCommandOrDie("IO.read", readArguments, onChunkRead);

    function onChunkRead(response)
    {
        if (had_eof)
            return;
        result += response.data;
        if (response.eof) {
            // Ignore stray callbacks from proactive read requests.
            had_eof = true;
            callback(result);
            return;
        }
        InspectorTest.sendCommandOrDie("IO.read", readArguments, onChunkRead);
    }
}

InspectorTest.findEvent = function(name, ph, condition)
{
    for (var i = 0; i < InspectorTest.devtoolsEvents.length; i++) {
        var e = InspectorTest.devtoolsEvents[i];
        if (e.name === name && e.ph === ph && (!condition || condition(e)))
            return e;
    }
    throw new Error("Couldn't find event " + name + " / " + ph + "\n\n in " + JSON.stringify(InspectorTest.devtoolsEvents, null, 2));
}

InspectorTest.invokeAsyncWithTracing = function(functionName, callback)
{
    InspectorTest.startTracing(onStart);

    function onStart()
    {
        InspectorTest.invokePageFunctionAsync(functionName, done);
    }

    function done()
    {
        InspectorTest.stopTracing(callback);
    }
}

InspectorTest._lastEvalId = 0;
InspectorTest._pendingEvalRequests = {};

InspectorTest.invokePageFunctionAsync = function(functionName, callback)
{
    var id = ++InspectorTest._lastEvalId;
    InspectorTest._pendingEvalRequests[id] = callback;
    var asyncEvalWrapper = function(callId, functionName)
    {
        function evalCallback(result)
        {
            evaluateInFrontend("InspectorTest.didInvokePageFunctionAsync(" + callId + ", " + JSON.stringify(result) + ");");
        }
        eval(functionName + "(" + evalCallback + ")");
    }
    InspectorTest.evaluateInPage("(" + asyncEvalWrapper.toString() + ")(" + id + ", unescape('" + escape(functionName) + "'))", function() { });
}

InspectorTest.didInvokePageFunctionAsync = function(callId, value)
{
    var callback = InspectorTest._pendingEvalRequests[callId];

    if (!callback) {
        InspectorTest.addResult("Missing callback for async eval " + callId + ", perhaps callback invoked twice?");
        return;
    }
    delete InspectorTest._pendingEvalRequests[callId];
    callback(value);
}

}
