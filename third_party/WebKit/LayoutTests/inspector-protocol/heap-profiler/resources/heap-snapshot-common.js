// This script is supposed to be evaluated in dummy inspector front-end which is loaded from
// ../../../http/tests/inspector-protocol/resources/protocol-test.html and the relative paths
// below are relative to that location.

if (!window.WebInspector)
    window.WebInspector = {};

self['Common'] = {};
self['HeapSnapshotModel'] = {};
self['HeapSnapshotWorker'] = {};

InspectorTest.importScript("../../../../../Source/devtools/front_end/platform/utilities.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/common/UIString.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/heap_snapshot_model/HeapSnapshotModel.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/heap_snapshot_worker/HeapSnapshot.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/common/TextUtils.js");
InspectorTest.importScript("../../../../../Source/devtools/front_end/heap_snapshot_worker/HeapSnapshotLoader.js");

InspectorTest.fail = function(message)
{
    InspectorTest.log("FAIL: " + message);
    InspectorTest.completeTest();
}

InspectorTest._takeHeapSnapshotInternal = function(command, callback)
{
    var loader = new HeapSnapshotWorker.HeapSnapshotLoader();
    InspectorTest.eventHandler["HeapProfiler.addHeapSnapshotChunk"] = function(messageObject)
    {
        loader.write(messageObject["params"]["chunk"]);
    }

    function didTakeHeapSnapshot(messageObject)
    {
        InspectorTest.log("Took heap snapshot");
        loader.close();
        var snapshot = loader.buildSnapshot(false);
        InspectorTest.log("Parsed snapshot");
        callback(snapshot);
    }
    InspectorTest.sendCommand(command, {}, didTakeHeapSnapshot);
}

InspectorTest.takeHeapSnapshot = function(callback)
{
    InspectorTest._takeHeapSnapshotInternal("HeapProfiler.takeHeapSnapshot", callback);
}

InspectorTest.stopRecordingHeapTimeline = function(callback)
{
    InspectorTest._takeHeapSnapshotInternal("HeapProfiler.stopTrackingHeapObjects", callback);
}
