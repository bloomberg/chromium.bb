function initialize_domTest()
{

InspectorTest.trackGetChildNodesEvents = function(nodeInfo, callback)
{
    InspectorTest.eventHandler["DOM.setChildNodes"] = setChildNodes;

    function setChildNodes(message)
    {
        var nodes = message.params.nodes;
        for (var i = 0; i < nodes.length; ++i)
            InspectorTest.addNode(nodeInfo, nodes[i]);
        if (callback)
            callback();
    }
}

InspectorTest.addNode = function(nodeInfo, node)
{
    nodeInfo[node.nodeId] = node;
    delete node.nodeId;
    var children = node.children || [];
    for (var i = 0; i < children.length; ++i)
        InspectorTest.addNode(nodeInfo, children[i]);
    var shadowRoots = node.shadowRoots || [];
    for (var i = 0; i < shadowRoots.length; ++i)
        InspectorTest.addNode(nodeInfo, shadowRoots[i]);
}

InspectorTest.requestDocumentNodeId = function(callback)
{
    InspectorTest.sendCommandOrDie("DOM.getDocument", {}, onGotDocument);

    function onGotDocument(result)
    {
        callback(result.root.nodeId);
    }
};

InspectorTest.requestNodeId = function(documentNodeId, selector, callback)
{
    InspectorTest.sendCommandOrDie("DOM.querySelector", { "nodeId": documentNodeId , "selector": selector }, onGotNode);

    function onGotNode(result)
    {
        callback(result.nodeId);
    }
};

}
