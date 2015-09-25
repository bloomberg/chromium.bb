// Asynchronous tests should manually call finishPaintInvalidationTest at the appropriate time.
window.testIsAsync = false;

// All paint invalidation tests are asynchronous from test-runner's point of view.
if (window.testRunner) {
    testRunner.waitUntilDone();
    if (document.location.hash == '#as-text')
        testRunner.dumpAsText();
}

function runPaintInvalidationTest()
{
    if (!window.testRunner || !window.internals) {
        setTimeout(paintInvalidationTest, 500);
        return;
    }

    // TODO(enne): this is a workaround for multiple svg onload events.
    // See: http://crbug.com/372946
    if (window.hasRunPaintInvalidationTest)
        return;
    window.hasRunPaintInvalidationTest = true;

    testRunner.layoutAndPaintAsyncThen(function()
    {
        window.internals.startTrackingPaintInvalidationObjects();
        paintInvalidationTest();
        if (!window.testIsAsync)
            finishPaintInvalidationTest();
    });
}

function removeAllChildren(element)
{
    while (element.firstChild)
        element.removeChild(element.firstChild);
}

function finishPaintInvalidationTest()
{
    if (!window.testRunner || !window.internals)
        return;

    testRunner.layoutAndPaintAsyncThen(function()
    {
        testRunner.setCustomTextOutput(JSON.stringify(internals.trackedPaintInvalidationObjects()).replace(/","/g, '",\n "'));
        testRunner.notifyDone();
    });
}
