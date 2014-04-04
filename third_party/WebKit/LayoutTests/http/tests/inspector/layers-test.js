function initialize_LayerTreeTests()
{
    // FIXME: remove once out of experimental.
    WebInspector.moduleManager.registerModule("layers");
    var extensions = WebInspector.moduleManager.extensions(WebInspector.Panel).forEach(function(extension) {
        if (extension.module().name() === "layers")
            WebInspector.inspectorView.addPanel(new WebInspector.ModuleManagerExtensionPanelDescriptor(extension));
    });
    InspectorTest.layerTreeModel = WebInspector.inspectorView.panel("layers")._model;

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
            root = InspectorTest.layerTreeModel.contentRoot();
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
            root = WebInspector.inspectorView.panel("layers")._layers3DView._rotatingContainerElement;
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
        InspectorTest.layerTreeModel.forEachLayer(testLayer);
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

    InspectorTest.dumpViewScrollRect = function(element)
    {
        var value = {
            className: element.className,
            title: element.title,
            width: element.style.width,
            height: element.style.height,
            left: element.style.left,
            top: element.style.top
        };
        if (element.__unchanged)
            value.__unchanged = element.__unchanged;
        InspectorTest.addObject(value, null, "", "scroll-rect: ");
    }

    InspectorTest.dumpViewScrollRects = function()
    {
        InspectorTest.addResult("View elements dump");
        var root = WebInspector.inspectorView.panel("layers")._layers3DView._rotatingContainerElement;
        Array.prototype.forEach.call(root.querySelectorAll('.scroll-rect'), InspectorTest.dumpViewScrollRect.bind(InspectorTest));
    }

    InspectorTest.dumpModelScrollRects = function()
    {
        function dumpScrollRectsForLayer(layer)
        {
            InspectorTest.addObject(layer._scrollRects);
        }

        InspectorTest.addResult("Model elements dump");
        InspectorTest.layerTreeModel.forEachLayer(dumpScrollRectsForLayer.bind(this));
    }
}
