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
        var paintInvalidationObjects = JSON.stringify(internals.trackedPaintInvalidationObjects());
        var expectedPaintInvalidationObjects = JSON.stringify(window.expectedPaintInvalidationObjects);
        internals.stopTrackingPaintInvalidationObjects();

        if (paintInvalidationObjects != expectedPaintInvalidationObjects) {
            testRunner.logToStderr('Paint invalidation objects mismatch.'
                + '\nExpected:\n' + expectedPaintInvalidationObjects.replace(/","/g, '",\n "')
                + '\nActual:\n' + paintInvalidationObjects.replace(/","/g, '",\n "'));

            if (document.rootElement && document.rootElement.nodeName == "svg") {
                var svgns = "http://www.w3.org/2000/svg"
                var rect = document.createElementNS(svgns, 'rect');
                rect.setAttribute('width', '45%');
                rect.setAttribute('height', '10%');
                rect.setAttribute('x', '55%');
                rect.setAttribute('fill', 'white');
                rect.setAttribute('stroke', 'black');
                rect.setAttribute('stroke-width', '1px');
                document.documentElement.appendChild(rect);
                var text = document.createElementNS(svgns, 'text');
                text.setAttribute('x', '100%');
                text.setAttribute('y', '5%');
                text.setAttribute('text-anchor', 'end');
                text.textContent = "Paint invalidation objects mismatch. See stderr for details.";
                document.documentElement.appendChild(text);
            } else {
                var div = document.createElement('div');
                div.style.zIndex = '2147483647';
                div.style.visibility = 'visible';
                div.style.position = 'fixed';
                div.style.top = '0';
                div.style.right = '0';
                div.style.width = '350px';
                div.style.height = '16px';
                div.style.color = 'black';
                div.style.fontSize = '12px';
                div.style.lineHeight = '14px';
                div.style.fontFamily = 'Arial';
                div.style.border = '1px solid black';
                div.style.background = 'white';
                div.appendChild(document.createTextNode('Paint invalidation objects mismatch. See stderr for details.'));
                document.body.appendChild(div);
            }
        }

        testRunner.notifyDone();
    });
}
