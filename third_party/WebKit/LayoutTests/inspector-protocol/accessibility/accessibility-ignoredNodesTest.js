// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initialize_IgnoredNodesTests() {
InspectorTest.dumpAccessibilityNodesByIdrefAndCompleteTest = function(ids, msg)
{
    if (msg.error) {
        InspectorTest.log(msg.error.message);
        InspectorTest.completeTest();
        return;
    }


    var rootNode = msg.result.root;
    var promises = [];
    for (var id of ids) {
        var selector = "#" + id;
        var promise = InspectorTest._sendQuerySelector(rootNode.nodeId, selector)
            .then(InspectorTest._getAXNode)
            .then(InspectorTest._rewriteNodes)
            .then(InspectorTest._dumpNode.bind(null, selector))
            .catch(function(msg) {
                       InspectorTest.log("Error: " + JSON.stringify(msg));
                   })
        promises.push(promise);
    }

    Promise.all(promises).then(function() { InspectorTest.completeTest(); });
}

InspectorTest._sendCommandPromise = function(command, params)
{
    return new Promise(function(resolve, reject) {
        InspectorTest.sendCommand(command, params, function(msg) {
            if (msg.error) {
                reject(msg.error);
                return;
            }

            resolve(msg);
        })
    });
}


InspectorTest._sendQuerySelector = function(nodeId, selector)
{
    return InspectorTest._sendCommandPromise("DOM.querySelector", { "nodeId": nodeId, "selector": selector });
}

InspectorTest._getAXNode = function(msg)
{
    var node = msg.result;
    return InspectorTest._sendCommandPromise("Accessibility.getAXNode", { "nodeId": node.nodeId });
}

InspectorTest._describeNode = function(nodeData)
{
    var description = nodeData.nodeName.toLowerCase();
    switch (nodeData.nodeType) {
    case Node.ELEMENT_NODE:
        var p = nodeData.attributes.indexOf("id");
        if (p >= 0)
            description += "#" + nodeData.attributes[p + 1];
    }
    return description;
}

InspectorTest._rewriteNode = function(relatedNode)
{
    function rewriteNodePromise(resolve, reject)
    {
        if (!("backendNodeId" in relatedNode)) {
            reject("Could not find backendNodeId in " + JSON.stringify(relatedNode));
            return;
        }
        var backendNodeId = relatedNode.backendNodeId;

        function onNodeResolved(backendNodeId, message)
        {
            var nodeId = message.result.nodeIds[0];
            if (!(nodeId in nodeInfo)) {
                relatedNode.nodeResult = "[NODE NOT FOUND]";
                resolve();
                return;
            }
            var node = nodeInfo[nodeId];
            delete relatedNode.backendNodeId;
            relatedNode.nodeResult = InspectorTest._describeNode(node);
            resolve();
        }
        InspectorTest.sendCommand("DOM.pushNodesByBackendIdsToFrontend", { "backendNodeIds": [ backendNodeId ] }, onNodeResolved.bind(null, backendNodeId));
    }
    return new Promise(rewriteNodePromise);
}

InspectorTest._checkExists = function(path, obj, reject)
{
    var pathComponents = path.split(".");
    var currentPath = [];
    var currentObject = obj;
    for (var component of pathComponents) {
        currentPath.push(component);
        if (!(component in currentObject)) {
            reject("Could not find " + currentPath.join(".") + " in " + JSON.stringify(obj, null, "  "));
            return false;
        }
        currentObject = currentObject[component];
    }
    return true;
}

InspectorTest._check = function(condition, errorMsg, obj, reject)
{
    if (condition)
        return true;
    reject(errorMsg + " in " + JSON.stringify(obj, null, "  "));
}

InspectorTest._rewriteNodes = function(msg)
{
    function rewriteNodesPromise(resolve, reject)
    {
        if (msg.error) {
            reject(msg.error.message);
            return;
        }

        if (msg.result.accessibilityNode.ignored) {
            InspectorTest._checkExists("result.accessibilityNode.ignoredReasons", msg);
            var properties = msg.result.accessibilityNode.ignoredReasons;
        } else {
            InspectorTest._checkExists("result.accessibilityNode.properties", msg);
            var properties = msg.result.accessibilityNode.properties;
        }
        var promises = [];
        for (var property of properties) {
            if (property.value.type === "idrefList" || property.value.type === "idref") {
                if (!InspectorTest._checkExists("value.relatedNodes", property, reject))
                    return;
                var relatedNodeArray = property.value.relatedNodes;
                InspectorTest._check(Array.isArray(relatedNodeArray), "value.relatedNodes should be an array", JSON.stringify(property), reject);
                 for (var relatedNode of relatedNodeArray)
                     promises.push(InspectorTest._rewriteNode(relatedNode));
            }
        }

        Promise.all(promises).then(resolve(msg));
    }
    return new Promise(rewriteNodesPromise);
}

InspectorTest._dumpNode = function(selector, msg)
{
    function stripIds(key, value)
    {
        if (key == "id")
            return "<int>"
        if (key == "backendNodeId")
            return "<string>"
        if (key == "nodeId")
            return "<string>"
        return value;
    }
    InspectorTest.log(selector + ": " + JSON.stringify(msg, stripIds, "  "));
}
}
