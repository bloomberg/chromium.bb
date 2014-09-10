function initialize_TracingTest()
{

// FIXME: remove when tracing is out of experimental
WebInspector.inspectorView.showPanel("timeline");
InspectorTest.tracingManager = WebInspector.panels.timeline._tracingManager || new WebInspector.TracingManager();
InspectorTest.tracingModel = new WebInspector.TracingModel();
InspectorTest.tracingTimelineModel = new WebInspector.TracingTimelineModel(InspectorTest.tracingManager, InspectorTest.tracingModel, new WebInspector.TimelineRecordHiddenTypeFilter([]));

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories)
{
    InspectorTest.tracingTimelineModel.addEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onTracingStarted, this);
    var categories = "-*,disabled-by-default-devtools.timeline*";
    if (additionalCategories)
        categories += "," + additionalCategories;
    InspectorTest.tracingTimelineModel._startRecordingWithCategories(categories);

    function onTracingStarted(event)
    {
        InspectorTest.tracingTimelineModel.removeEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onTracingStarted, this);
        InspectorTest.invokePageFunctionAsync(functionName, onPageActionsDone);
    }

    function onPageActionsDone()
    {
        InspectorTest.tracingTimelineModel.addEventListener(WebInspector.TimelineModel.Events.RecordingStopped, onTracingComplete, this);
        InspectorTest.tracingTimelineModel.stopRecording();
    }

    function onTracingComplete(event)
    {
        InspectorTest.tracingTimelineModel.removeEventListener(WebInspector.TimelineModel.Events.RecordingStopped, onTracingComplete, this);
        callback();
    }
}

}
