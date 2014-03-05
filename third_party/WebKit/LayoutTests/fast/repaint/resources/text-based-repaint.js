// Asynchronous tests should manually call finishRepaintTest at the appropriate time.
window.testIsAsync = false;

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

    if (document.body)
        document.body.offsetTop;
    else if (document.documentElement)
        document.documentElement.offsetTop;

    window.internals.startTrackingRepaints(document);

    repaintTest();

    if (!window.testIsAsync)
        finishRepaintTest();
}

function finishRepaintTest()
{
    // Force a style recalc.
    var dummy = document.body.offsetTop;

    var repaintRects = window.internals.repaintRectsAsText(document);

    internals.stopTrackingRepaints(document);

    var pre = document.createElement('pre');
    document.body.appendChild(pre);
    pre.textContent += repaintRects;

    if (window.afterTest)
        window.afterTest();

    if (window.testIsAsync)
        testRunner.notifyDone();
}
