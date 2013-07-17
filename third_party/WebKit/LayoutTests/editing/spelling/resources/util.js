function log(msg)
{
    document.getElementById("console").innerHTML += (msg + "\n");
}

function verifySpellTest(nretry)
{
    var node = window.destination;
    if (window.destination.childNodes.length > 0)
        node = window.destination.childNodes[0];
    if (nretry && !internals.markerCountForNode(node, "spelling")) {
        window.setTimeout(function() { verifySpellTest(nretry - 1); }, 0);
        return;
    }
    testFunctionCallback(node);
    finishJSTest()
}

function initSpellTest(testElementId, testText, testFunction)
{
    if (!window.internals || !window.testRunner) {
        log("FAIL Incomplete test environment");
        return;
    }
    testFunctionCallback = testFunction;
    jsTestIsAsync = true;
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
    internals.settings.setSmartInsertDeleteEnabled(true);
    internals.settings.setSelectTrailingWhitespaceEnabled(false);
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setEditingBehavior("win");
    window.destination = document.getElementById(testElementId);
    window.destination.focus();
    document.execCommand("InsertText", false, testText);
    window.setTimeout(function() { verifySpellTest(10); }, 0);
}
