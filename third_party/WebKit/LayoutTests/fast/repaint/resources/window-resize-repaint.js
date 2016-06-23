var testSizes = [
    { width: 600, height: 500 }, // initial size
    { width: 600, height: 250 }, // height decrease
    { width: 400, height: 250 }, // width decrease
    { width: 400, height: 600 }, // height increase
    { width: 800, height: 600 }  // width increase
];

var sizeIndex = 0;
var repaintRects = "";

if (window.internals) {
    internals.settings.setUseDefaultImageInterpolationQuality(true);
    // TODO(wangxianzhu): Some spv2 tests crash with under-invalidation-checking
    // because the extra display items between Subsequence/EndSubsequence for
    // under-invalidation checking breaks paint chunks. Should fix this when fixing
    // crbug.com/596983.
    if (!internals.runtimeFlags.slimmingPaintV2Enabled)
        internals.runtimeFlags.slimmingPaintUnderInvalidationCheckingEnabled = true;
}

function doTest() {
    if (sizeIndex) {
        repaintRects += internals.layerTreeAsText(document, window.internals.LAYER_TREE_INCLUDES_PAINT_INVALIDATIONS);
        internals.stopTrackingRepaints(document);
    }
    ++sizeIndex;
    if (sizeIndex < testSizes.length) {
        internals.startTrackingRepaints(document);
        window.resizeTo(testSizes[sizeIndex].width, testSizes[sizeIndex].height);
        testRunner.layoutAndPaintAsyncThen(doTest);
    } else if (window.testRunner) {
        testRunner.setCustomTextOutput(repaintRects);
        testRunner.notifyDone();
    }
}

if (window.testRunner) {
    testRunner.useUnfortunateSynchronousResizeMode();
    testRunner.waitUntilDone();
    window.resizeTo(testSizes[0].width, testSizes[0].height);
    testRunner.layoutAndPaintAsyncThen(doTest);
}
