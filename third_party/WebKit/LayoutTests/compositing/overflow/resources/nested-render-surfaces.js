if (window.testRunner)
    testRunner.dumpAsTextWithPixelResults();

function runTest(description) {
    var pre = document.createElement("pre");
    document.body.appendChild(pre);

    if (!window.internals) {
        pre.innerHTML = description;
    } else {
        var isUnclipped = window.internals.isUnclippedDescendant(document.getElementById('fixed'));
        pre.innerHTML = "\n'fixed' " + (isUnclipped ? "is" : "is not") + " an unclipped descendant.";

        if (isUnclipped) {
            pre.innerHTML += "\nPASS";
        } else {
            pre.innerHTML += "\nUnclipped Descendants are only relevant for " +
                "preferCompositingToLCDText, so this result is " +
                "expected everywhere except in the virtual/gpu/ suite.";
        }
    }
}
