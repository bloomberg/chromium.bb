function initialize_AuditTests()
{

InspectorTest.preloadPanel("audits");

InspectorTest.collectAuditResults = function()
{
    WebInspector.panels.audits.showResults(WebInspector.panels.audits.auditResultsTreeElement.children[0].results);
    var liElements = WebInspector.panels.audits.visibleView.element.getElementsByTagName("li");
    for (var j = 0; j < liElements.length; ++j) {
        if (liElements[j].treeElement)
            liElements[j].treeElement.expand();
    }
    InspectorTest.collectTextContent(WebInspector.panels.audits.visibleView.element, "");
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
    var child = element.firstChild;

    while (child) {
        if (child.nodeType === Node.TEXT_NODE) {
            nodeOutput += child.nodeValue.replace("\u200B", "");
        } else if (child.nodeType === Node.ELEMENT_NODE) {
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
