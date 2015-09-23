function loadSecondIFrame()
{
    document.getElementById("myframe").onload = null;
    document.getElementById("myframe").src = "resources/iframe-load-event-iframe-2.html";
}

function test()
{
    InspectorTest.expandElementsTree(step1);

    function step1()
    {
        InspectorTest.domModel.addEventListener(WebInspector.DOMModel.Events.NodeInserted, nodeInserted);
        InspectorTest.evaluateInPage("loadSecondIFrame()");

        function nodeInserted(event)
        {
            var node = event.data;
            if (node.getAttribute("id") === "myframe") {
                InspectorTest.expandElementsTree(step2);
                InspectorTest.domModel.removeEventListener(WebInspector.DOMModel.Events.NodeInserted, nodeInserted);
            }
        }
    }

    function step2()
    {
        InspectorTest.addResult("\n\nAfter frame navigate");
        InspectorTest.dumpElementsTree();
        InspectorTest.completeTest();
    }
}
