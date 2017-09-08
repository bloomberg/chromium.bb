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
    var decoratePromise = InspectorTest.addSnifferPromise(Coverage.CoverageView.LineDecorator.prototype, "_innerDecorate");
    var sourceFrame = await new Promise(fulfill => InspectorTest.showScriptSource(source, fulfill));
    await decoratePromise;
    return sourceFrame;
}

InspectorTest.dumpDecorations = async function(source)
{
    var sourceFrame = await InspectorTest.sourceDecorated(source);
    InspectorTest.dumpDecorationsInSourceFrame(sourceFrame);
}

InspectorTest.findCoverageNodeForURL = function(url)
{
    var coverageListView = self.runtime.sharedInstance(Coverage.CoverageView)._listView;
    var rootNode = coverageListView._dataGrid.rootNode();
    for (var child of rootNode.children) {
      if (child._coverageInfo.url().endsWith(url))
        return child;
    }
    return null;
}

InspectorTest.dumpDecorationsInSourceFrame = function(sourceFrame)
{
    var markerMap = new Map([['used', '+'], ['unused', '-']]);

    var codeMirror = sourceFrame.textEditor.codeMirror();
    for (var line = 0; line < codeMirror.lineCount(); ++line) {
        var text = codeMirror.getLine(line);
        var markerType = ' ';
        var lineInfo = codeMirror.lineInfo(line);
        if (!lineInfo)
            continue;
        var gutterElement = lineInfo.gutterMarkers && lineInfo.gutterMarkers['CodeMirror-gutter-coverage'];
        if (gutterElement) {
            var markerClass = /^text-editor-coverage-(\w*)-marker$/.exec(gutterElement.classList)[1];
            markerType = markerMap.get(markerClass) || gutterElement.classList;
        }
        InspectorTest.addResult(`${line}: ${markerType} ${text}`);
    }
}

InspectorTest.dumpCoverageListView = function()
{
    var coverageListView = self.runtime.sharedInstance(Coverage.CoverageView)._listView;
    var dataGrid = coverageListView._dataGrid;
    dataGrid.updateInstantly();
    for (var child of dataGrid.rootNode().children) {
        var data = child._coverageInfo;
        var url = InspectorTest.formatters.formatAsURL(data.url());
        if (url.endsWith("-test.js") || url.endsWith(".html"))
            continue;
        var type = Coverage.CoverageListView._typeToString(data.type());
        InspectorTest.addResult(`${url} ${type} used: ${data.usedSize()} unused: ${data.unusedSize()} total: ${data.size()}`);
    }
}

}
