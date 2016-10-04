function isUsingCompositedScrolling(layers) {
    var foundScrollingContentsLayer = false;
    layers["layers"].forEach(function(layer) {
        if (layer.name == "Scrolling Contents Layer")
            foundScrollingContentsLayer = true;
    });

    return foundScrollingContentsLayer;
}
