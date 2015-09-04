// Asynchronous tests should manually call finishPaintInvalidationTest at the appropriate time.
window.testIsAsync = false;

// All paint invalidation tests are asynchronous from test-runner's point of view.
if (window.testRunner)
    testRunner.waitUntilDone();

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
        document.body.offsetTop;
        var paintInvalidationObjects = JSON.stringify(internals.trackedPaintInvalidationObjects());
        var expectedPaintInvalidationObjects = JSON.stringify(window.expectedPaintInvalidationObjects);
        internals.stopTrackingPaintInvalidationObjects();

        if (paintInvalidationObjects != expectedPaintInvalidationObjects) {
            var message = 'Paint invalidation objects mismatch.'
                + '\nExpected:\n' + expectedPaintInvalidationObjects.replace(/","/g, '",\n "')
                + '\nActual:\n' + paintInvalidationObjects.replace(/","/g, '",\n "');
            testRunner.logToStderr(message);
            while (document.body.firstChild)
                document.body.removeChild(document.body.firstChild);
            var pre = document.createElement('pre');
            pre.appendChild(document.createTextNode('(To copy results as text, see stderr.)\n\n' + message));
            document.body.appendChild(pre);
        }
        testRunner.notifyDone();
    });
}
