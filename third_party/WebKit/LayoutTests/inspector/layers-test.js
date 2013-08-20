function initialize_LayerTreeTests()
{
    // FIXME: remove once out of experimental.
    WebInspector.inspectorView.addPanel(new WebInspector.LayersPanelDescriptor());
    InspectorTest._layerTreeModel = WebInspector.panel("layers")._model;

    InspectorTest.labelForLayer = function(layer)
    {
        var node = WebInspector.domAgent.nodeForId(layer.nodeIdForSelfOrAncestor());
        var label = node.appropriateSelectorFor(false);
        var height = layer.height();
        var width = layer.width();
        if (height <= 200 && width <= 200)
            label += " " + height + "x" + width;
        if (typeof layer.__extraData !== "undefined")
            label += " (" + layer.__extraData + ")";
        return label;
    }

    InspectorTest.dumpLayerTree = function(prefix, root)
    {
        if (!prefix)
            prefix = "";
        if (!root) {
            root = InspectorTest._layerTreeModel.root();
            if (!root) {
                InspectorTest.addResult("No layer root, perhaps not in the composited mode! ");
                InspectorTest.completeTest();
                return;
            }
        }
        InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root));
        root.children().forEach(InspectorTest.dumpLayerTree.bind(InspectorTest, prefix + "    "));
    }

    InspectorTest.evaluateAndRunWhenTreeChanges = function(expression, callback)
    {
        function eventHandler()
        {
            InspectorTest._layerTreeModel.removeEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, eventHandler);
            callback();
        }
        InspectorTest._layerTreeModel.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, eventHandler);
        InspectorTest.evaluateInPage(expression, function() {});
    }

    InspectorTest.findLayerByNodeIdAttribute = function(nodeIdAttribute)
    {
        var result;
        function testLayer(layer)
        {
            if (!layer.nodeId())
                return false;
            var node = WebInspector.domAgent.nodeForId(layer.nodeId());
            if (!node || node.getAttribute("id") !== nodeIdAttribute)
                return false;
            result = layer;
            return true;
        }
        InspectorTest._layerTreeModel.forEachLayer(testLayer);
        return result;
    }
}
