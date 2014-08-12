function initialize_LayerTreeTests()
{
    // FIXME: remove once out of experimental.
    InspectorTest.registerModule("layers");
    var extensions = runtime.extensions(WebInspector.Panel).forEach(function(extension) {
        if (extension.module().name() === "layers")
            WebInspector.inspectorView.addPanel(new WebInspector.RuntimeExtensionPanelDescriptor(extension));
    });
    InspectorTest.layerTreeModel = WebInspector.inspectorView.panel("layers")._model;
    InspectorTest.layers3DView = WebInspector.inspectorView.panel("layers")._layers3DView;

    InspectorTest.labelForLayer = function(layer)
    {
        var node = layer.nodeForSelfOrAncestor();
        var label = node ? WebInspector.DOMPresentationUtils.fullQualifiedSelector(node, false) : "<invalid node id>";
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
            root = InspectorTest.layerTreeModel.layerTree().contentRoot();
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
            root = WebInspector.inspectorView.panel("layers")._layerTree._treeOutline;
        if (root.representedObject)
            InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root.representedObject));
        root.children.forEach(InspectorTest.dumpLayerTreeOutline.bind(InspectorTest, prefix + "    "));
    }

    InspectorTest.dumpLayers3DView = function(prefix, root)
    {
        if (!prefix)
            prefix = "";
        if (!root)
            root = InspectorTest.layers3DView._rotatingContainerElement;
        if (root.__layer)
            InspectorTest.addResult(prefix + InspectorTest.labelForLayer(root.__layer));
        for (var element = root.firstElementChild; element; element = element.nextSibling)
            InspectorTest.dumpLayers3DView(prefix + "    ", element);
    }

    InspectorTest.evaluateAndRunWhenTreeChanges = function(expression, callback)
    {
        function eventHandler()
        {
            InspectorTest.layerTreeModel.removeEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, eventHandler);
            callback();
        }
        InspectorTest.evaluateInPage(expression, function() {
            InspectorTest.layerTreeModel.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, eventHandler);
        });
    }

    InspectorTest.findLayerByNodeIdAttribute = function(nodeIdAttribute)
    {
        var result;
        function testLayer(layer)
        {
            var node = layer.node();
            if (!node)
                return false;
            if (!node || node.getAttribute("id") !== nodeIdAttribute)
                return false;
            result = layer;
            return true;
        }
        InspectorTest.layerTreeModel.layerTree().forEachLayer(testLayer);
        if (!result)
            InspectorTest.addResult("ERROR: No layer for " + nodeIdAttribute);
        return result;
    }

    InspectorTest.requestLayers = function(callback)
    {
        InspectorTest.layerTreeModel.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, onLayerTreeChanged);
        InspectorTest.layerTreeModel.enable();
        function onLayerTreeChanged()
        {
            InspectorTest.layerTreeModel.removeEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged, onLayerTreeChanged);
            callback();
        }
    }

    InspectorTest.dumpModelScrollRects = function()
    {
        function dumpScrollRectsForLayer(layer)
        {
            if (layer._scrollRects.length > 0)
                InspectorTest.addObject(layer._scrollRects);
        }

        InspectorTest.addResult("Model elements dump");
        InspectorTest.layerTreeModel.layerTree().forEachLayer(dumpScrollRectsForLayer.bind(this));
    }

    InspectorTest.dispatchMouseEvent = function(eventType, button, element, offsetX, offsetY)
    {
        var totalOffset = element.totalOffset();
        var scrollOffset = element.scrollOffset();
        var eventArguments = {
            bubbles: true,
            cancelable: true,
            view: window,
            screenX: totalOffset.left - scrollOffset.left + offsetX,
            screenY: totalOffset.top - scrollOffset.top + offsetY,
            clientX: totalOffset.left + offsetX,
            clientY: totalOffset.top + offsetY,
            button: button
        };
        if (eventType === "mouseout") {
            eventArguments.screenX = 0;
            eventArguments.screenY = 0;
            eventArguments.clientX = 0;
            eventArguments.clientY = 0;
        }
        element.dispatchEvent(new MouseEvent(eventType, eventArguments));
    }
}
