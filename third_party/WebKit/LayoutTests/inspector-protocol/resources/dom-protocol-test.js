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

InspectorTest.requestDocumentNodeId = async function(callback)
{
    var result = await InspectorTest.sendCommandOrDie("DOM.getDocument", {});
    if (callback)
      callback(result.root.nodeId);
    return result.root.nodeId;
};

InspectorTest.requestNodeId = async function(documentNodeId, selector, callback)
{
    var result = await InspectorTest.sendCommandOrDie("DOM.querySelector", { "nodeId": documentNodeId , "selector": selector });
    if (callback)
      callback(result.nodeId);
    return result.nodeId;
};

}
