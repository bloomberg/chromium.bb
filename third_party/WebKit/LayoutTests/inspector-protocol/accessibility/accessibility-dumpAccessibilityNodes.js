// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initialize_DumpAccessibilityNodesTest() {

var nodeInfo = {};
InspectorTest.trackGetChildNodesEvents(nodeInfo);

InspectorTest.dumpAccessibilityNodesBySelectorAndCompleteTest = function(selector, msg) {
    if (msg.error) {
        InspectorTest.log(msg.error.message);
        InspectorTest.completeTest();
        return;
    }

    var rootNode = msg.result.root;
    sendQuerySelectorAll(rootNode.nodeId, selector)
        .then((msg) => { return getAXNodes(msg) } )
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

function getAXNodes(msg)
{
    var promise;
    msg.result.nodeIds.forEach((id) => {
        if (promise)
            promise = promise.then(() => { return sendCommandPromise("Accessibility.getAXNodeChain", { "nodeId": id, "fetchAncestors": false }); });
        else
            promise = sendCommandPromise("Accessibility.getAXNodeChain", { "nodeId": id, "fetchAncestors": false });
        promise = promise.then((msg) => { return rewriteRelatedNodes(msg); })
                         .then((msg) => { return dumpNode(null, msg); });
    });
    return promise;
}

function describeRelatedNode(nodeData)
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

function rewriteRelatedNode(relatedNode)
{
    function rewriteRelatedNodePromise(resolve, reject)
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
            relatedNode.nodeResult = describeRelatedNode(node);
            resolve();
        }
        InspectorTest.sendCommand("DOM.pushNodesByBackendIdsToFrontend", { "backendNodeIds": [ backendNodeId ] }, onNodeResolved.bind(null, backendNodeId));
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

function rewriteRelatedNodes(msg)
{
    if (msg.error) {
        throw new Error(msg.error.message);
    }

    var node = msg.result.nodes[0];

    if (node.ignored) {
        checkExists("result.nodes[0].ignoredReasons", msg);
        var properties = node.ignoredReasons;
    } else {
        checkExists("result.nodes[0].properties", msg);
        var properties = node.properties;
    }
    var promises = [];
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
    return Promise.all(promises).then(() => { return msg; });
}

function dumpNode(selector, msg)
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
    InspectorTest.log((selector ? selector + ": " : "") + JSON.stringify(msg, stripIds, "  "));
}
}
