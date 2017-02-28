function initialize_CoverageTests() {

InspectorTest.preloadModule("coverage");

InspectorTest.startCoverage = function()
{
    UI.viewManager.showView("coverage");
    var coverageView = self.runtime.sharedInstance(Coverage.CoverageView);
    coverageView._startRecording();
}

InspectorTest.stopCoverage = function()
{
    var coverageView = self.runtime.sharedInstance(Coverage.CoverageView);
    return coverageView._stopRecording();
}

InspectorTest.sourceDecorated = async function(source)
{
    await UI.inspectorView.showPanel("sources");
    var decoratePromise = InspectorTest.addSnifferPromise(Coverage.CoverageView.LineDecorator.prototype, "decorate");
    await new Promise(fulfill => InspectorTest.showScriptSource(source, fulfill));
    await decoratePromise;
}

}
