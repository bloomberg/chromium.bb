function test()
{
    var containerNode;
    var child1Node;
    var child2Node;
    var aNode;

    InspectorTest.expandElementsTree(step0);

    function step0()
    {
        containerNode = InspectorTest.expandedNodeWithId("container");
        child1Node = InspectorTest.expandedNodeWithId("child1");
        child2Node = InspectorTest.expandedNodeWithId("child2");
        aNode = InspectorTest.expandedNodeWithId("aNode");

        aNode.setMarker("attr1", true);
        InspectorTest.addResult("attr1 set on aNode");
        InspectorTest.dumpElementsTree(null);

        child2Node.setMarker("attr2", "value");
        InspectorTest.addResult("attr2 set on child2");
        InspectorTest.dumpElementsTree(null);

        child2Node.setMarker("attr1", true);
        InspectorTest.addResult("attr1 set on child2");
        InspectorTest.dumpElementsTree(null);

        aNode.setMarker("attr1", "anotherValue");
        InspectorTest.addResult("attr1 modified on aNode");
        InspectorTest.dumpElementsTree(null);

        child2Node.setMarker("attr2", "anotherValue");
        InspectorTest.addResult("attr2 modified on child2");
        InspectorTest.dumpElementsTree(null);

        aNode.setMarker("attr1", null);
        InspectorTest.addResult("attr1 removed from aNode");
        InspectorTest.dumpElementsTree(null);

        aNode.removeNode(step1);
    }

    function step1(error)
    {
        if (error) {
            InspectorTest.addResult("Failed to remove aNode");
            InspectorTest.completeTest();
            return;
        }

        InspectorTest.addResult("aNode removed");
        InspectorTest.dumpElementsTree(null);

        child2Node.removeNode(step2);
    }

    function step2(error)
    {
        if (error) {
            InspectorTest.addResult("Failed to remove child2");
            InspectorTest.completeTest();
            return;
        }

        InspectorTest.addResult("child2 removed");
        InspectorTest.dumpElementsTree(null);
        InspectorTest.completeTest();
    }
}
