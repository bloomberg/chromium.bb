function initialize_CoverageTests() {

InspectorTest.preloadModule("coverage");

InspectorTest.sourceDecorated = async function(source) {

    await UI.inspectorView.showPanel("sources");
    var decoratePromise = InspectorTest.addSnifferPromise(Coverage.CoverageView.LineDecorator.prototype, "decorate");
    await new Promise(fulfill => InspectorTest.showScriptSource(source, fulfill));
    await decoratePromise;
}

}
