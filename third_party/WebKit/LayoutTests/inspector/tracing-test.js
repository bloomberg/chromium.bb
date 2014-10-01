function initialize_TracingTest()
{

InspectorTest.preloadPanel("timeline");

InspectorTest.tracingManager = function()
{
    if (WebInspector.panels.timeline._tracingManager)
        return WebInspector.panels.timeline._tracingManager;
    if (!InspectorTest._tracingManager)
        InspectorTest._tracingManager = new WebInspector.TracingManager();
    return InspectorTest._tracingManager;
}

InspectorTest.tracingModel = function()
{
    if (!InspectorTest._tracingModel)
        InspectorTest._tracingModel = new WebInspector.TracingModel();
    return InspectorTest._tracingModel;
}

InspectorTest.tracingTimelineModel = function()
{
    if (!InspectorTest._tracingTimelineModel)
        InspectorTest._tracingTimelineModel = new WebInspector.TracingTimelineModel(InspectorTest.tracingManager(), InspectorTest.tracingModel(), new WebInspector.TimelineRecordHiddenTypeFilter([]));
    return InspectorTest._tracingTimelineModel;
}

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories)
{
    InspectorTest.tracingTimelineModel().addEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onTracingStarted, this);
    var categories = "-*,disabled-by-default-devtools.timeline*";
    if (additionalCategories)
        categories += "," + additionalCategories;
    InspectorTest.tracingTimelineModel()._startRecordingWithCategories(categories);

    function onTracingStarted(event)
    {
        InspectorTest.tracingTimelineModel().removeEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onTracingStarted, this);
        InspectorTest.invokePageFunctionAsync(functionName, onPageActionsDone);
    }

    function onPageActionsDone()
    {
        InspectorTest.tracingTimelineModel().addEventListener(WebInspector.TimelineModel.Events.RecordingStopped, onTracingComplete, this);
        InspectorTest.tracingTimelineModel().stopRecording();
    }

    function onTracingComplete(event)
    {
        InspectorTest.tracingTimelineModel().removeEventListener(WebInspector.TimelineModel.Events.RecordingStopped, onTracingComplete, this);
        callback();
    }
}

}
