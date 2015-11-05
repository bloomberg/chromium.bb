function initialize_TracingTest()
{

InspectorTest.preloadPanel("timeline");
WebInspector.TempFile = InspectorTest.TempFileMock;

InspectorTest.createTracingModel = function()
{
    return new WebInspector.TracingModel(new WebInspector.TempFileBackingStorage("tracing"));
}

InspectorTest.tracingModel = function()
{
    return WebInspector.panels.timeline._tracingModel;
}

InspectorTest.tracingTimelineModel = function()
{
    return WebInspector.panels.timeline._model;
}

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories, enableJSSampling)
{
    var categories = "-*,disabled-by-default-devtools.timeline*,devtools.timeline," + WebInspector.TracingModel.TopLevelEventCategory;
    if (additionalCategories)
        categories += "," + additionalCategories;
    InspectorTest.tracingTimelineModel()._startRecordingWithCategories(categories, enableJSSampling, tracingStarted);

    function tracingStarted()
    {
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
