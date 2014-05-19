function initialize_TracingTest()
{

// FIXME: remove when tracing is out of experimental
WebInspector.inspectorView.showPanel("timeline");
InspectorTest.tracingModel = new WebInspector.TracingModel();

InspectorTest.invokeWithTracing = function(categoryFilter, functionName, callback)
{
    InspectorTest.tracingModel.start(categoryFilter, "", onTracingStarted);

    function onTracingStarted(error)
    {
        InspectorTest.invokePageFunctionAsync(functionName, onPageActionsDone)
    }

    function onPageActionsDone()
    {
        InspectorTest.tracingModel.stop(callback);
    }
}

}
