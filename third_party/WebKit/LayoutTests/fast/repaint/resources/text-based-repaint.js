// Asynchronous tests should manually call finishRepaintTest at the appropriate
// time.
window.testIsAsync = false;
window.outputRepaintRects = true;

// All repaint tests are asynchronous.
if (window.testRunner)
    testRunner.waitUntilDone();

if (window.internals) {
    internals.settings.setUseDefaultImageInterpolationQuality(true);
    // TODO(wangxianzhu): Some spv2 tests crash with under-invalidation-checking
    // because the extra display items between Subsequence/EndSubsequence for
    // under-invalidation checking breaks paint chunks. Should fix this when fixing
    // crbug.com/596983.
    if (!internals.runtimeFlags.slimmingPaintV2Enabled)
        internals.runtimeFlags.slimmingPaintUnderInvalidationCheckingEnabled = true;
}

function runRepaintTest()
{
    if (!window.testRunner || !window.internals) {
        setTimeout(repaintTest, 500);
        return;
    }

    if (window.enablePixelTesting)
        testRunner.dumpAsTextWithPixelResults();
    else
        testRunner.dumpAsText();

    testRunner.layoutAndPaintAsyncThen(function() {
        window.internals.startTrackingRepaints(document);
        repaintTest();
        if (!window.testIsAsync)
            finishRepaintTest();
    });
}

function runRepaintAndPixelTest()
{
    window.enablePixelTesting = true;
    runRepaintTest();
}

function forceStyleRecalc()
{
    if (document.body)
        document.body.clientTop;
    else if (document.documentElement)
        document.documentElement.clientTop;
}

function finishRepaintTest()
{
    if (!window.testRunner || !window.internals)
        return;

    // Force a style recalc.
    forceStyleRecalc();

    var flags = window.internals.LAYER_TREE_INCLUDES_PAINT_INVALIDATIONS;

    if (window.layerTreeAsTextAdditionalFlags)
        flags |= window.layerTreeAsTextAdditionalFlags;

    var repaintRects = window.internals.layerTreeAsText(document, flags);

    internals.stopTrackingRepaints(document);

    // Play nice with JS tests which may want to print out assert results.
    if (window.isJsTest)
        window.outputRepaintRects = false;

    if (window.outputRepaintRects)
        testRunner.setCustomTextOutput(repaintRects);

    if (window.afterTest)
        window.afterTest();

    // Play nice with async JS tests which want to notifyDone themselves.
    if (!window.jsTestIsAsync)
        testRunner.notifyDone();
}
