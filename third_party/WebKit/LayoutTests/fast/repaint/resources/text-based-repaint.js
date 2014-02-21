function runRepaintTest()
{
    if (window.testRunner && window.internals) {
        if (window.enablePixelTesting)
            window.testRunner.dumpAsTextWithPixelResults();
        else
            window.testRunner.dumpAsText();

        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        window.internals.startTrackingRepaints(document);

        repaintTest();

        // force a style recalc.
        var dummy = document.body.offsetTop;

        var repaintRects = window.internals.repaintRectsAsText(document);

        window.internals.stopTrackingRepaints(document);

        var pre = document.createElement('pre');
        document.body.appendChild(pre);
        pre.textContent += repaintRects;

        if (window.afterTest)
            window.afterTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
