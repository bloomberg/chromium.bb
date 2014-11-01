// This script is supposed to be evaluated in dummy inspector front-end which is loaded from
// ../../../http/tests/inspector-protocol/resources/protocol-test.html and the relative paths
// below are relative to that location.

if (!window.WebInspector)
    window.WebInspector = {};
InspectorTest.importScript("../../../../../Source/devtools/front_end/common/UIString.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/common/utilities.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/profiler/HeapSnapshotCommon.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/heap_snapshot_worker/HeapSnapshot.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/heap_snapshot_worker/JSHeapSnapshot.js");

InspectorTest.fail = function(message)
{
    InspectorTest.log("FAIL: " + message);
    InspectorTest.completeTest();
}

InspectorTest.assert = function(result, message)
{
    if (!result)
        InspectorTest.fail(message);
}

InspectorTest.takeHeapSnapshot = function(callback)
{
    var chunks = [];
    InspectorTest.eventHandler["HeapProfiler.addHeapSnapshotChunk"] = function(messageObject)
    {
        chunks.push(messageObject["params"]["chunk"]);
    }

    function didTakeHeapSnapshot(messageObject)
    {
        var serializedSnapshot = chunks.join("");
        var parsed = JSON.parse(serializedSnapshot);
        parsed.nodes = new Uint32Array(parsed.nodes);
        parsed.edges = new Uint32Array(parsed.edges);
        var snapshot = new WebInspector.JSHeapSnapshot(parsed, new WebInspector.HeapSnapshotProgress());
        callback(snapshot);
        InspectorTest.log("SUCCESS: didGetHeapSnapshot");
        InspectorTest.completeTest();
    }
    InspectorTest.sendCommand("HeapProfiler.takeHeapSnapshot", {}, didTakeHeapSnapshot);
}
