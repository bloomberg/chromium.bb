// Asynchronous tests should manually call finishRepaintTest at the appropriate
// time.
window.testIsAsync = false;
window.outputRepaintRects = true;
window.generateMinimumRepaint = false; // See comments about 'Minimum repaint' below.

// All repaint tests are asynchronous.
if (window.testRunner)
    testRunner.waitUntilDone();

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

    function continueRepaintTest()
    {
        window.internals.startTrackingRepaints(document);
        repaintTest();
        if (!window.testIsAsync)
            finishRepaintTest();
    }

    if (window.generateMinimumRepaint) {
        testRunner.capturePixelsAsyncThen(function(width, height, snapshot) {
            window.widthBeforeRepaint = width;
            window.heightBeforeRepaint = height;
            window.snapshotBeforeRepaint = snapshot;
            continueRepaintTest();
        });
    } else {
        testRunner.layoutAndPaintAsyncThen(continueRepaintTest);
    };
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

    var flags = window.internals.LAYER_TREE_INCLUDES_REPAINT_RECTS |
        window.internals.LAYER_TREE_INCLUDES_PAINT_INVALIDATION_OBJECTS;

    if (window.layerTreeAsTextAdditionalFlags)
        flags |= window.layerTreeAsTextAdditionalFlags;

    var repaintRects = window.internals.layerTreeAsText(document, flags);

    internals.stopTrackingRepaints(document);

    function repaintTestDone()
    {
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

    if (window.generateMinimumRepaint) {
        internals.forceFullRepaint(document);
        testRunner.capturePixelsAsyncThen(function(width, height, snapshot) {
            if (window.outputRepaintRects) {
                var minimumRepaint = computeMinimumRepaint(width, height, snapshot);
                if (minimumRepaint.length)
                    repaintRects += '\nMinimum repaint:\n' + JSON.stringify(minimumRepaint, null, 2);
            }
            repaintTestDone();
        });
    } else {
        repaintTestDone();
    }
}

// Minimum repaint
//
// Definitions
// - minimum-repaint = region(diff(snapshot-before-repaintTest,
//                                 snapshot-after-repaintTest-and-full-repaint))
//   It includes all pixels that should be changed after repaintTest.
// - actual-repaint = repaint rects recorded during repaintTest() and
//   forceStyleRecalc().
// - potential-under-repaint = subtract(minimum-repaint, actual-repaint)
//   Potential-under-repaint will be shown in layout test overlay in black if
//   any minimum-repaint region is not covered by actual-repaint.
//
// Potential-under-repaints don't always mean bug:
// - Some know visualization issues (crbug.com/381221) may cause false
//   under-repaint;
// - Screen updates caused by composited layer re-compositing may not need
//   repaint.
//
// How to use
// 1. Set window.generateMinimumRepaint to true in some repaint test or change
//    this script to force window.generateMinimumRepaint to true.
// 2. Run layout tests. Repaint tests will result text diffs, which is because
//    of the minimum repaint output in the actual results and doesn't mean the
//    tests failed.
// 3. In layout test result page, click 'overlay' link or expand the test to
//    see the 'overlay' pane.
// 4. Click 'Highlight under-repaint' button. You'll see black region if there
//    is any under-repaint.
//
// Known issues
//    crbug.com/381221

function computeMinimumRepaint(width, height, snapshot)
{
    var result = [];
    if (width > widthBeforeRepaint) {
        result.push([widthBeforeRepaint, 0, width - widthBeforeRepaint, Math.max(height, heightBeforeRepaint)]);
        width = widthBeforeRepaint;
    }
    if (height > heightBeforeRepaint) {
        result.push([0, heightBeforeRepaint, width, height - heightBeforeRepaint]);
        height = heightBeforeRepaint;
    }

    var dataBefore = new Uint32Array(snapshotBeforeRepaint);
    var dataAfter = new Uint32Array(snapshot);
    var rectsMayContinue = [];
    var index = 0;
    for (var y = 0; y < height; ++y) {
        var x = 0;
        var rectsMayContinueIndex = 0;
        var nextRectsMayContinue = [];
        while (true) {
            while (x < width && dataBefore[index] == dataAfter[index]) {
                ++x;
                ++index;
            }
            xBegin = x;
            while (x < width && dataBefore[index] != dataAfter[index]) {
                ++x;
                ++index;
            }
            xEnd = x;

            var xWidth = xEnd - xBegin;
            if (!xWidth)
                break;

            var rectMayContinue = rectsMayContinue[rectsMayContinueIndex];
            while (rectMayContinue && rectMayContinue[0] < xBegin)
                rectMayContinue = rectsMayContinue[++rectsMayContinueIndex];

            if (rectMayContinue && rectMayContinue[0] == xBegin && rectMayContinue[2] == xWidth) {
                ++rectMayContinue[3];
                nextRectsMayContinue.push(rectMayContinue);
            } else {
                var newRect = [xBegin, y, xWidth, 1];
                nextRectsMayContinue.push(newRect);
                result.push(newRect);
            }
        }
        rectsMayContinue = nextRectsMayContinue;
    }
    return result;
}
