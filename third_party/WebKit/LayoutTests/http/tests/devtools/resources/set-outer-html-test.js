var initialize_SetOuterHTMLTest = function() {

InspectorTest.events = [];
InspectorTest.containerId;

InspectorTest.setUpTestSuite = function(next)
{
    InspectorTest.expandElementsTree(step1);

    function step1()
    {
        InspectorTest.selectNodeWithId("container", step2);
    }

    function step2(node)
    {
        InspectorTest.containerId = node.id;
        InspectorTest.DOMAgent.getOuterHTML(InspectorTest.containerId).then(step3);
    }

    function step3(text)
    {
        InspectorTest.containerText = text;

        for (var key in SDK.DOMModel.Events) {
            var eventName = SDK.DOMModel.Events[key];
            if (eventName === SDK.DOMModel.Events.MarkersChanged || eventName === SDK.DOMModel.Events.DOMMutated)
                continue;
            InspectorTest.domModel.addEventListener(eventName, InspectorTest.recordEvent.bind(InspectorTest, eventName));
        }

        next();
    }
}

InspectorTest.recordEvent = function(eventName, event)
{
    if (!event.data)
        return;
    var node = event.data.node || event.data;
    var parent = event.data.parent;
    for (var currentNode = parent || node; currentNode; currentNode = currentNode.parentNode) {
        if (currentNode.getAttribute("id") === "output")
            return;
    }
    InspectorTest.events.push("Event " + eventName.toString() + ": " + node.nodeName());
}

InspectorTest.patchOuterHTML = function(pattern, replacement, next)
{
    InspectorTest.addResult("Replacing '" + pattern + "' with '" + replacement + "'\n");
    InspectorTest.setOuterHTML(InspectorTest.containerText.replace(pattern, replacement), next);
}

InspectorTest.patchOuterHTMLUseUndo = function(pattern, replacement, next)
{
    InspectorTest.addResult("Replacing '" + pattern + "' with '" + replacement + "'\n");
    InspectorTest.setOuterHTMLUseUndo(InspectorTest.containerText.replace(pattern, replacement), next);
}

InspectorTest.setOuterHTML = function(newText, next)
{
    InspectorTest.innerSetOuterHTML(newText, false, bringBack);

    function bringBack()
    {
        InspectorTest.addResult("\nBringing things back\n");
        InspectorTest.innerSetOuterHTML(InspectorTest.containerText, true, next);
    }
}

InspectorTest.setOuterHTMLUseUndo = function(newText, next)
{
    InspectorTest.innerSetOuterHTML(newText, false, bringBack);

    async function bringBack()
    {
        InspectorTest.addResult("\nBringing things back\n");
        await InspectorTest.domModel.undo();
        InspectorTest._dumpOuterHTML(true, next);
    }
}

InspectorTest.innerSetOuterHTML = async function(newText, last, next)
{
    await InspectorTest.DOMAgent.setOuterHTML(InspectorTest.containerId, newText);
    InspectorTest._dumpOuterHTML(last, next);
}

InspectorTest._dumpOuterHTML = async function(last, next)
{
    var result = await InspectorTest.RuntimeAgent.evaluate("document.getElementById(\"identity\").wrapperIdentity");

    InspectorTest.addResult("Wrapper identity: " + result.value);
    InspectorTest.events.sort();
    for (var i = 0; i < InspectorTest.events.length; ++i)
        InspectorTest.addResult(InspectorTest.events[i]);
    InspectorTest.events = [];

    var text = await InspectorTest.DOMAgent.getOuterHTML(InspectorTest.containerId);

    InspectorTest.addResult("==========8<==========");
    InspectorTest.addResult(text);
    InspectorTest.addResult("==========>8==========");
    if (last)
        InspectorTest.addResult("\n\n\n");
    next();
}

};
