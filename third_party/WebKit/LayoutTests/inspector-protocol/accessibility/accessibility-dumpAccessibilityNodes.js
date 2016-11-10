// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initialize_DumpAccessibilityNodesTest() {

var nodeInfo = {};
InspectorTest.trackGetChildNodesEvents(nodeInfo);

InspectorTest.dumpAccessibilityNodesBySelectorAndCompleteTest = function(selector, fetchRelatives, msg) {
    if (msg.error) {
        InspectorTest.log(msg.error.message);
        InspectorTest.completeTest();
        return;
    }

    var rootNode = msg.result.root;
    var rootNodeId = rootNode.nodeId;
    InspectorTest.addNode(nodeInfo, rootNode);

    sendQuerySelectorAll(rootNodeId, selector)
        .then((msg) => { return getAXNodes(msg, fetchRelatives || false) } )
        .then(() => { done(); })
        .then(() => {
            InspectorTest.completeTest();
        })
        .catch((msg) => { InspectorTest.log("Error: " + JSON.stringify(msg)); })
}

function sendCommandPromise(command, params)
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

function done()
{
    sendCommandPromise("Runtime.evaluate", { expression: "done();" });
}

function sendQuerySelectorAll(nodeId, selector)
{
    return sendCommandPromise("DOM.querySelectorAll", { "nodeId": nodeId, "selector":
 selector });
}

function getAXNodes(msg, fetchRelatives)
{
    var promise = Promise.resolve();
    if (!msg.result || !msg.result.nodeIds) {
        InspectorTest.log("Unexpected result: " + JSON.stringify(msg));
        InspectorTest.completeTest();
    }
    msg.result.nodeIds.forEach((id) => {
        if (fetchRelatives) {
            promise = promise.then(() => {
                return sendCommandPromise("Accessibility.getPartialAXTree", { "nodeId": id, "fetchRelatives": true });
            });
            promise = promise.then((msg) => { return rewriteRelatedNodes(msg, id); })
                             .then((msg) => { return dumpTreeStructure(msg); });

        }
        promise = promise.then(() => { return sendCommandPromise("Accessibility.getPartialAXTree", { "nodeId": id, "fetchRelatives": false }); })
                         .then((msg) => { return rewriteRelatedNodes(msg, id); })
                         .then((msg) => { return dumpNode(msg); });

    });
    return promise;
}

function describeDomNode(nodeData)
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

function rewriteBackendDomNodeId(axNode, selectedNodeId, promises)
{
    if (!("backendDOMNodeId" in axNode))
        return;

    function rewriteBackendDomNodeIdPromise(resolve, reject)
    {
        if (!("backendDOMNodeId" in axNode)) {
            resolve();
            return;
        }
        var backendDOMNodeId = axNode.backendDOMNodeId;

        function onDomNodeResolved(backendDOMNodeId, message)
        {
            if (!message.result || !message.result.nodeIds) {
                InspectorTest.log("Unexpected result for pushNodesByBackendIdsToFrontend: " + JSON.stringify(message));
                InspectorTest.completeTest();
                return;
            }
            var nodeId = message.result.nodeIds[0];
            if (!(nodeId in nodeInfo)) {
                axNode.domNode = "[NODE NOT FOUND]";
                resolve();
                return;
            }
            var domNode = nodeInfo[nodeId];
            delete axNode.backendDOMNodeId;
            axNode.domNode = describeDomNode(domNode);
            if (nodeId === selectedNodeId)
              axNode.selected = true;
            resolve();
        }

        var params = { "backendNodeIds": [ backendDOMNodeId ] };
        InspectorTest.sendCommand("DOM.pushNodesByBackendIdsToFrontend", params , onDomNodeResolved.bind(null, backendDOMNodeId));
    }
    promises.push(new Promise(rewriteBackendDomNodeIdPromise));
}

function rewriteRelatedNode(relatedNode)
{
    function rewriteRelatedNodePromise(resolve, reject)
    {
        if (!("backendDOMNodeId" in relatedNode)) {
            reject("Could not find backendDOMNodeId in " + JSON.stringify(relatedNode));
            return;
        }
        var backendDOMNodeId = relatedNode.backendDOMNodeId;

        function onNodeResolved(backendDOMNodeId, message)
        {
            if (!message.result || !message.result.nodeIds) {
                InspectorTest.log("Unexpected result for pushNodesByBackendIdsToFrontend: " + JSON.stringify(message));
                InspectorTest.completeTest();
                return;
            }
            var nodeId = message.result.nodeIds[0];
            if (!(nodeId in nodeInfo)) {
                relatedNode.nodeResult = "[NODE NOT FOUND]";
                resolve();
                return;
            }
            var domNode = nodeInfo[nodeId];
            delete relatedNode.backendDOMNodeId;
            relatedNode.nodeResult = describeDomNode(domNode);
            resolve();
        }
        var params = { "backendNodeIds": [ backendDOMNodeId ] };
        InspectorTest.sendCommand("DOM.pushNodesByBackendIdsToFrontend", params, onNodeResolved.bind(null, backendDOMNodeId));

    }
    return new Promise(rewriteRelatedNodePromise);
}

function checkExists(path, obj)
{
    var pathComponents = path.split(".");
    var currentPath = [];
    var currentObject = obj;
    for (var component of pathComponents) {
        var isArray = false;
        var index = -1;
        var matches = component.match(/(\w+)\[(\d+)\]/);
        if (matches) {
            isArray = true;
            component = matches[1];
            index = Number.parseInt(matches[2], 10);
        }
        currentPath.push(component);
        if (!(component in currentObject)) {
            InspectorTest.log("Could not find " + currentPath.join(".") + " in " + JSON.stringify(obj, null, "  "));
            InspectorTest.completeTest();
        }
        if (isArray)
            currentObject = currentObject[component][index];
        else
            currentObject = currentObject[component];
    }
    return true;
}

function check(condition, errorMsg, obj)
{
    if (condition)
        return true;
    throw new Error(errorMsg + " in " + JSON.stringify(obj, null, "  "));
}

function rewriteRelatedNodeValue(value, promises)
{
    checkExists("relatedNodes", value);
    var relatedNodeArray = value.relatedNodes;
    check(Array.isArray(relatedNodeArray), "relatedNodes should be an array", JSON.stringify(value));
    for (var relatedNode of relatedNodeArray) {
        promises.push(rewriteRelatedNode(relatedNode));
    }
}

function rewriteRelatedNodes(msg, nodeId)
{
    if (msg.error) {
        throw new Error(msg.error.message);
    }

    var promises = [];
    for (var node of msg.result.nodes) {
        if (node.ignored) {
            checkExists("ignoredReasons", node);
            var properties = node.ignoredReasons;
        } else {
            checkExists("properties", node);
            var properties = node.properties;
        }
        if (node.name && node.name.sources) {
            for (var source of node.name.sources) {
            var value;
                if (source.value)
                    value = source.value;
                if (source.attributeValue)
                    value = source.attributeValue;
                if (!value)
                    continue;
                if (value.type === "idrefList" ||
                    value.type === "idref" ||
                    value.type === "nodeList")
                    rewriteRelatedNodeValue(value, promises);
            }
        }
        for (var property of properties) {
            if (property.value.type === "idrefList" ||
                property.value.type === "idref" ||
                property.value.type === "nodeList")
                rewriteRelatedNodeValue(property.value, promises);
        }
        rewriteBackendDomNodeId(node, nodeId, promises);
    }
    return Promise.all(promises).then(() => { return msg; });
}

function dumpNode(msg)
{
    function stripIds(key, value)
    {
        var stripKeys = ["id", "backendDOMNodeId", "nodeId", "parentId"];
        if (stripKeys.indexOf(key) !== -1)
            return "<" + typeof(value) + ">";
        var deleteKeys = ["selected"];
        if (deleteKeys.indexOf(key) !== -1)
            return undefined;
        return value;
    }
    if (!msg.result || !msg.result.nodes || msg.result.nodes.length !== 1) {
        InspectorTest.log("Expected exactly one node in " + JSON.stringify(msg, null, "  "));
        return;
    }
    InspectorTest.log(JSON.stringify(msg.result.nodes[0], stripIds, "  "));
}

function dumpTreeStructure(msg)
{
    function printNodeAndChildren(node, leadingSpace)
    {
        leadingSpace = leadingSpace || "";
        var string = leadingSpace;
        if (node.selected)
            string += "*";
        if (node.role)
            string += node.role.value;
        else
            string += "<no role>";
        string += (node.name && node.name.value !== "" ? " \"" + node.name.value + "\"" : "");
        if (node.children) {
            for (var child of node.children)
                string += "\n" + printNodeAndChildren(child, leadingSpace + "  ");
        }
        return string;
    }

    var rawMsg = JSON.stringify(msg, null, "  ");
    var nodeMap = {};
    if ("result" in msg && "nodes" in msg.result) {
        for (var node of msg.result.nodes)
            nodeMap[node.nodeId] = node;
    }
    for (var nodeId in nodeMap) {
        var node = nodeMap[nodeId];
        if (node.childIds) {
            node.children = [];
            for (var i = 0; i < node.childIds.length && node.childIds.length > 0;) {
                var childId = node.childIds[i];
                if (childId in nodeMap) {
                    var child = nodeMap[childId];
                    child.parentId = nodeId;
                    node.children.push(child);

                    node.childIds.splice(i, 1);
                } else {
                    node.childIds[i] = "<string>";
                    i++;
                }
            }
            if (!node.childIds.length)
                delete node.childIds;
            if (!node.children.length)
                delete node.children;
        }
    }
    var rootNode = Object.values(nodeMap).find((node) => !("parentId" in node));
    for (var node of Object.values(nodeMap))
        delete node.parentId;

    InspectorTest.log("\n" + printNodeAndChildren(rootNode));
}

}
