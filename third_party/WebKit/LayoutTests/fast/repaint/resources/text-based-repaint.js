// Asynchronous tests should manually call finishRepaintTest at the appropriate time.
window.testIsAsync = false;
window.outputRepaintRects = true;

if (window.internals)
    window.internals.settings.setForceCompositingMode(true)

function runRepaintTest()
{
    if (!window.testRunner || !window.internals) {
        setTimeout(repaintTest, 100);
        return;
    }

    if (window.enablePixelTesting)
        testRunner.dumpAsTextWithPixelResults();
    else
        testRunner.dumpAsText();

    if (window.testIsAsync)
        testRunner.waitUntilDone();

    forceStyleRecalc();

    window.internals.startTrackingRepaints(document);

    repaintTest();

    if (!window.testIsAsync)
        finishRepaintTest();
}

function runRepaintAndPixelTest()
{
    window.enablePixelTesting = true;
    window.outputRepaintRects = false;
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
    // Force a style recalc.
    forceStyleRecalc();

    var repaintRects = window.internals.layerTreeAsText(document, window.internals.LAYER_TREE_INCLUDES_REPAINT_RECTS);

    internals.stopTrackingRepaints(document);

    if (window.outputRepaintRects)
        testRunner.setCustomTextOutput(repaintRects);

    if (window.afterTest)
        window.afterTest();

    if (window.testIsAsync)
        testRunner.notifyDone();
}
