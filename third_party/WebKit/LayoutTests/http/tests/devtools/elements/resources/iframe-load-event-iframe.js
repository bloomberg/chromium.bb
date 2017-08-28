function loadSecondIFrame()
{
    document.getElementById("myframe").src = "resources/iframe-load-event-iframe-2.html";
    return new Promise((resolve) => document.getElementById("myframe").onload = resolve);
}

function test()
{
    InspectorTest.expandElementsTree(step1);

    function step1()
    {
        InspectorTest.evaluateInPageAsync("loadSecondIFrame()").then(() => InspectorTest.expandElementsTree(step2));
    }

    function step2()
    {
        InspectorTest.addResult("\n\nAfter frame navigate");
        InspectorTest.dumpElementsTree();
        InspectorTest.completeTest();
    }
}
