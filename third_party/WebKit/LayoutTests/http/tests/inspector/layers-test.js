function initialize_LayerTreeTests()
{
    // FIXME: remove once out of experimental.
    WebInspector.moduleManager.registerModule("layers");
    var extensions = WebInspector.moduleManager.extensions(WebInspector.Panel).forEach(function(extension) {
        if (extension.module().name() === "layers")
            WebInspector.inspectorView.addPanel(new WebInspector.ModuleManagerExtensionPanelDescriptor(extension));
    });
    InspectorTest._layerTreeModel = WebInspector.panel("layers")._model;

    InspectorTest.labelForLayer = function(layer)
    {
        var node = WebInspector.domAgent.nodeForId(layer.nodeIdForSelfOrAncestor());
        var label = WebInspector.DOMPresentationUtils.appropriateSelectorFor(node, false);
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
            root = InspectorTest._layerTreeModel.contentRoot();
            if (!root) {
                InspectorTest.addResult("No layer root, perhaps not in the composited mode! ");
                InspectorTest.completeTest();
                return;
            }
        }
        InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root));
        root.children().forEach(InspectorTest.dumpLayerTree.bind(InspectorTest, prefix + "    "));
    }

    InspectorTest.dumpLayerTreeOutline = function(prefix, root)
    {
        if (!prefix)
            prefix = "";
        if (!root)
            root = WebInspector.panel("layers")._layerTree._treeOutline;
        if (root.representedObject)
            InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root.representedObject));
        root.children.forEach(InspectorTest.dumpLayerTreeOutline.bind(InspectorTest, prefix + "    "));
    }

    InspectorTest.dumpLayers3DView = function(prefix, root)
    {
        if (!prefix)
            prefix = "";
        if (!root)
            root = WebInspector.panel("layers")._layers3DView._rotatingContainerElement;
        if (root.__layer)
            InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root.__layer));
        for (var element = root.firstElementChild; element; element = element.nextSibling)
            InspectorTest.dumpLayers3DView(prefix + "    ", element);
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
        if (!result)
            InspectorTest.addResult("ERROR: No layer for " + nodeIdAttribute);
        return result;
    }

    InspectorTest.requestLayers = function(callback)
    {
        InspectorTest._layerTreeModel.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, onLayerTreeChanged);
        InspectorTest._layerTreeModel.enable();
        function onLayerTreeChanged()
        {
            InspectorTest._layerTreeModel.removeEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, onLayerTreeChanged);
            callback();
        }
    }

}
