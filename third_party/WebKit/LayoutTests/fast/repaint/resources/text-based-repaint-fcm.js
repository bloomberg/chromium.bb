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

    if (window.outputRepaintRects) {
        if (document.body) {
            var root = document.body;
        } else if (document.documentElement) {
            var root = document.createElementNS('http://www.w3.org/2000/svg', 'foreignObject');
            document.documentElement.appendChild(root);
        }
        var pre = document.createElementNS('http://www.w3.org/1999/xhtml', 'pre');
        // Make this element appear in text dumps, but try to avoid affecting
        // output pixels (being visible, creating overflow, &c).
        pre.style.opacity = 0;
        pre.textContent += repaintRects;
        root.appendChild(pre);
    }

    if (window.afterTest)
        window.afterTest();

    if (window.testIsAsync)
        testRunner.notifyDone();
}
