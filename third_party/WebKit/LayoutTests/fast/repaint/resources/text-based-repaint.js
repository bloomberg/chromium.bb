// Asynchronous tests should manually call finishRepaintTest at the appropriate
// time.
window.testIsAsync = false;
window.outputRepaintRects = true;

function runRepaintTest()
{
    if (!window.testRunner || !window.internals) {
        setTimeout(repaintTest, 500);
        return;
    }

    // TODO(enne): this is a workaround for multiple svg onload events.
    // See: http://crbug.com/372946
    if (window.hasRunRepaintTest)
        return;
    window.hasRunRepaintTest = true;

    if (window.enablePixelTesting)
        testRunner.dumpAsTextWithPixelResults();
    else
        testRunner.dumpAsText();

    // All repaint tests are asynchronous.
    testRunner.waitUntilDone();

    testRunner.displayAsyncThen(function() {
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
        document.body.offsetTop;
    else if (document.documentElement)
        document.documentElement.offsetTop;
}

function finishRepaintTest()
{
    if (!window.testRunner || !window.internals)
        return;

    // Force a style recalc.
    forceStyleRecalc();

    var repaintRects = window.internals.layerTreeAsText(document, window.internals.LAYER_TREE_INCLUDES_REPAINT_RECTS);

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
