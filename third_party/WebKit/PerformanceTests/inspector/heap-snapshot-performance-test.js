function test()
{
    WebInspector.showPanel("profiles");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildEdgeIndexes");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildRetainers");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildDominatedNodes");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_calculateFlags");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildAggregates");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_calculateClassesRetainedSize");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_calculateDistances");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_calculateRetainedSizes");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_markDetachedDOMTreeNodes");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_markQueriableHeapObjects");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_markPageOwnedNodes");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_splitNodesAndContainmentEdges");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildPostOrderIndex");
    InspectorTest.measureFunction(WebInspector.JSHeapSnapshot.prototype, "_buildDominatorTree");
    InspectorTest.measureFunction(WebInspector.HeapSnapshotConstructorsDataGrid.prototype, "_aggregatesReceived");

    function performanceTest(timer)
    {
        var transferTimerCookie;
        var showTimerCookie;
        var changeViewTimerCookie;
        var clearTimerCookie;

        var testName = /([^\/]+)\.html$/.exec(WebInspector.inspectedPageURL)[1];
        var fullTimerCookie = timer.start("full-summary-snapshot-time");
        var type = WebInspector.panels.profiles.getProfileType("HEAP");

        var backendTimerCookie = timer.start("take-snapshot");
        type._takeHeapSnapshot(step0);

        function step0()
        {
            timer.finish(backendTimerCookie);
            transferTimerCookie = timer.start("transfer-snapshot");
            var profiles = type.getProfiles();
            InspectorTest.addSniffer(profiles[0]._transferHandler, "_updateProgress", onUpdateProgress, true);
            InspectorTest.addSniffer(profiles[0], "_wasShown", step2);
        }

        function onUpdateProgress(saved, total)
        {
            if (saved !== total)
                return;
            timer.finish(transferTimerCookie);
            showTimerCookie = timer.start("show-snapshot");
        }

        function step2()
        {
            timer.finish(showTimerCookie);
            changeViewTimerCookie = timer.start("switch-to-containment-view");
            InspectorTest.switchToView("Containment", cleanup);
        }

        function cleanup()
        {
            timer.finish(changeViewTimerCookie);
            timer.finish(fullTimerCookie);
            clearTimerCookie = timer.start("clear-snapshot");
            HeapProfilerAgent.clearProfiles(done);
            WebInspector.panels.profiles._reset();
        }

        function done()
        {
            timer.finish(clearTimerCookie);
            timer.done(testName);
        }
    }

    InspectorTest.runPerformanceTest(performanceTest, 60000);
}
