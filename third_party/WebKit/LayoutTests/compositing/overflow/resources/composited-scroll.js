function isUsingCompositedScrolling(layers) {
    var foundScrollingContentsLayer = false;
    layers["layers"].forEach(function(layer) {
        if (layer.name == "Scrolling Contents Layer")
            foundScrollingContentsLayer = true;
    });

    return foundScrollingContentsLayer;
}

function hasOpaqueCompositedScrollingContentsLayer(layers) {
    var found = false;
    layers["layers"].forEach(function(layer) {
      if (layer.name == "Scrolling Contents Layer")
        found = found || layer.contentsOpaque;
    });
    return found;
}

function hasNotOpaqueCompositedScrollingContentsLayer(layers) {
    var found = false;
    layers["layers"].forEach(function(layer) {
      if (layer.name == "Scrolling Contents Layer")
        found = found || !layer.contentsOpaque;
    });
    return found;
}
