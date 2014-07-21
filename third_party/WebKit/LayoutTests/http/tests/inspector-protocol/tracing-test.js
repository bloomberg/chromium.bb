// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var evalCallbackCallId = 3;

initialize_tracingHarness = function()
{

InspectorTest.startTracing = function(callback)
{
    InspectorTest.sendCommand("Tracing.start", { "categories": "-*,disabled-by-default-devtools.timeline", "type": "", "options": "" }, onStart);

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
    InspectorTest.sendCommand("Tracing.end", { }, onStop);

    InspectorTest.devtoolsEvents = [];
    function dataCollected(reply)
    {
        var allEvents = reply.params.value;
        InspectorTest.devtoolsEvents = InspectorTest.devtoolsEvents.concat(allEvents.filter(function(e)
        {
            return e.cat === "disabled-by-default-devtools.timeline";
        }));
    }

    function tracingComplete(event)
    {
        InspectorTest.log("Tracing complete");
        InspectorTest.eventHandler["Tracing.tracingComplete"] = null;
        InspectorTest.eventHandler["Tracing.dataCollected"] = null;
        callback(InspectorTest.devtoolsEvents);
    }

    function onStop(response)
    {
        InspectorTest.log("Recording stopped");
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

