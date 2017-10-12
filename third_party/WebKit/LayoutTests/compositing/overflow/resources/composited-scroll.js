function elementSubtreeHasCompositedScrollLayers(element) {
    var layerTree = window.internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layers = JSON.parse(layerTree);
    var foundScrollingContentsLayer = false;
    layers["layers"].forEach(function(layer) {
        if (layer.name == "Scrolling Contents Layer")
            foundScrollingContentsLayer = true;
    });

    return foundScrollingContentsLayer;
}

function elementSubtreeHasOpaqueCompositedScrollingContentsLayer(element) {
    var layerTree = window.internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layers = JSON.parse(layerTree);
    var found = false;
    layers["layers"].forEach(function(layer) {
      if (layer.name == "Scrolling Contents Layer")
        found = found || layer.contentsOpaque;
    });
    return found;
}

function elementSubtreeHasNotOpaqueCompositedScrollingContentsLayer(element) {
    var layerTree = window.internals.elementLayerTreeAsText(element);
    if (layerTree === '')
        return false;
    var layers = JSON.parse(layerTree);
    var found = false;
    layers["layers"].forEach(function(layer) {
      if (layer.name == "Scrolling Contents Layer")
        found = found || !layer.contentsOpaque;
    });
    return found;
}
