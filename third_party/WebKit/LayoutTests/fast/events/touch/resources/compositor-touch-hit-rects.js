function listener() {
}

function log(msg) {
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span);
    span.innerHTML = msg + '<br />';
}

function nameForNode(node) {
    if (!node)
        return "[unknown-node]";
    var name = node.nodeName;
    if (node.id)
        name += '#' + node.id;
   return name;
}

function sortRects(a, b) {
    return a.layerRelativeRect.top - b.layerRelativeRect.top
        || a.layerRelativeRect.left - b.layerRelativeRect.left
        || a.layerRelativeRect.width - b.layerRelativeRect.width
        || a.layerRelativeRect.height - b.layerRelativeRect.right
        || nameForNode(a.layerRootNode).localeCompare(nameForNode(b.layerRootNode))
        || a.layerType.localeCompare(b.layerType);
}

var preRunHandlerForTest = {};

function testElement(element) {
    element.addEventListener('touchstart', listener, false);

    // Run any test-specific handler AFTER adding the touch event listener
    // (which itself causes rects to be recomputed).
    if (element.id in preRunHandlerForTest)
        preRunHandlerForTest[element.id](element);

    logRects(element.id);
    element.removeEventListener('touchstart', listener, false);
}

function logRects(testName, opt_noOverlay) {
    if (!window.internals) {
        log(testName + ': not run');
        return;
    }

    var rects = window.internals.touchEventTargetLayerRects(document);
    if (rects.length == 0)
        log(testName + ': no rects');

    var sortedRects = new Array();
    for ( var i = 0; i < rects.length; ++i)
        sortedRects[i] = rects[i];
    sortedRects.sort(sortRects);
    for ( var i = 0; i < sortedRects.length; ++i) {
        var node = sortedRects[i].layerRootNode;
        var r = sortedRects[i].layerRelativeRect;
        var nameSuffix = "";
        if (sortedRects[i].layerType)
            nameSuffix = " " + sortedRects[i].layerType;
        log(testName + ": " + nameForNode(node) + nameSuffix + " ("
            + r.left + ", " + r.top + ", " + r.width + ", " + r.height + ")");

        if (visualize && node && !opt_noOverlay && window.location.hash != '#nooverlay') {
            var patch = document.createElement("div");
            patch.className = "overlay generated display-when-done";
            patch.style.left = r.left + "px";
            patch.style.top = r.top + "px";
            patch.style.width = r.width + "px";
            patch.style.height = r.height + "px";

            if (node === document) {
                patch.style.position = "absolute";
                document.body.appendChild(patch);
            } else {
                // Use a zero-size container to avoid changing the position of
                // the existing elements.
                var container = document.createElement("div");
                container.className = "overlay-container generated";
                patch.style.position = "relative";
                node.appendChild(container);
                container.style.top = (node.offsetTop - container.offsetTop) + "px";
                container.style.left = (node.offsetLeft - container.offsetLeft) + "px";
                container.classList.add("display-when-done");
                container.appendChild(patch);
            }
        }
    }

    log('');
}

function checkForRectUpdate(expectUpdate, operation) {
    if (window.internals)
        var oldCount = window.internals
                .touchEventTargetLayerRectsUpdateCount(document);

    operation();

    if (window.internals) {
        var newCount = window.internals
                .touchEventTargetLayerRectsUpdateCount(document);
        if ((oldCount != newCount) != !!expectUpdate)
            log('FAIL: ' + (expectUpdate ? 'rects not updated' : 'rects updated unexpectedly'));
    }
}

// Set this to true in order to visualize the results in an image.
// Elements that are expected to be included in hit rects have a red border.
// The actual hit rects are in a green tranlucent overlay.
var visualize = false;

if (window.testRunner) {
    if (vistualize)
        window.testRunner.dumpAsTextWithPixelResults();
    else
        window.testRunner.dumpAsText();
    document.documentElement.setAttribute('dumpRenderTree', 'true');
} else {
    // Note, this test can be run interactively in content-shell with
    // --expose-internals-for-testing.  In that case we almost certainly
    // want to visualize the results.
    visualize = true;
}

if (window.internals) {
    window.internals.settings.setMockScrollbarsEnabled(true);
    window.internals.settings.setForceCompositingMode(true);
}

window.onload = function() {
    // Run each general test case.
    var tests = document.querySelectorAll('.testcase');
    for ( var i = 0; i < tests.length; i++)
        testElement(tests[i]);

    if (window.additionalTests)
        additionalTests();

    if (!visualize && window.internals) {
        var testContainer = document.getElementById("tests");
        testContainer.parentNode.removeChild(testContainer);
    }

    document.documentElement.setAttribute('done', 'true');
};
