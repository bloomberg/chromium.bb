function initialize_AuditTests()
{

InspectorTest.preloadPanel("audits");

InspectorTest.collectAuditResults = function(callback)
{
    WebInspector.panels.audits.showResults(WebInspector.panels.audits.auditResultsTreeElement.firstChild().results);
    var trees = WebInspector.panels.audits.visibleView.element.querySelectorAll(".audit-result-tree");
    for (var i = 0; i < trees.length; ++i) {
        var liElements = trees[i].shadowRoot.getElementsByTagName("li");
        for (var j = 0; j < liElements.length; ++j) {
            if (liElements[j].treeElement)
                liElements[j].treeElement.expand();
        }
    }
    InspectorTest.runAfterPendingDispatches(function() {
        InspectorTest.collectTextContent(WebInspector.panels.audits.visibleView.element, "");
        callback();
    });
}

InspectorTest.launchAllAudits = function(shouldReload, callback)
{
    InspectorTest.addSniffer(WebInspector.AuditController.prototype, "_auditFinishedCallback", callback);
    var launcherView = WebInspector.panels.audits._launcherView;
    launcherView._selectAllClicked(true);
    launcherView._auditPresentStateElement.checked = !shouldReload;
    launcherView._launchButtonClicked();
}

InspectorTest.collectTextContent = function(element, indent)
{
    var nodeOutput = "";
    var child = element.shadowRoot || element.firstChild;

    var nonTextTags = { "STYLE": 1, "SCRIPT": 1 };
    while (child) {
        if (child.nodeType === Node.TEXT_NODE) {
            if (!nonTextTags[child.parentElement.nodeName])
                nodeOutput += child.nodeValue.replace("\u200B", "");
        } else if (child.nodeType === Node.ELEMENT_NODE || child.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
            if (nodeOutput !== "") {
                InspectorTest.addResult(indent + nodeOutput);
                nodeOutput = "";
            }
            if (!child.firstChild && child.className.indexOf("severity") == 0)
                nodeOutput = "[" + child.className + "] ";
            else
                InspectorTest.collectTextContent(child, indent + " ");
        }
        child = child.nextSibling;
    }
    if (nodeOutput !== "")
        InspectorTest.addResult(indent + nodeOutput);
}

}
