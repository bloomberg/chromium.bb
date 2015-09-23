var testSizes = [
    { width: 200, height: 200 }, // initial size
    { width: 200, height: 300 }, // height increase
    { width: 300, height: 300 }, // width increase
    { width: 300, height: 250 }, // height decrease
    { width: 250, height: 250 }  // width decrease
    // Tests can add more testSizes.
];

var sizeIndex = 0;

function repaintTest() {
    window.resizeTo(testSizes[sizeIndex].width, testSizes[sizeIndex].height);
}

if (window.internals) {
    internals.settings.setOverlayScrollbarsEnabled(true);
    internals.settings.setMockScrollbarsEnabled(true);
}

if (window.testRunner) {
    testRunner.useUnfortunateSynchronousResizeMode();
    testRunner.dumpAsText();

    window.onload = function() {
        window.resizeTo(testSizes[0].width, testSizes[0].height);

        var repaintRects = "";
        for (sizeIndex = 1; sizeIndex < testSizes.length; ++sizeIndex) {
            document.body.offsetTop;
            internals.startTrackingRepaints(document);
            repaintTest();
            document.body.offsetTop;
            repaintRects += internals.layerTreeAsText(document, window.internals.LAYER_TREE_INCLUDES_REPAINT_RECTS | window.internals.LAYER_TREE_INCLUDES_PAINT_INVALIDATION_OBJECTS);
            internals.stopTrackingRepaints(document);
        }
        testRunner.setCustomTextOutput(repaintRects);
    }
}
