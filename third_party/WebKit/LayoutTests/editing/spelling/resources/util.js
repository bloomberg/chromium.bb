function log(msg)
{
    document.getElementById("console").innerHTML += (msg + "\n");
}

function verifySpellTest(nretry)
{
    var node = destination;
    if (destination.childNodes.length > 0)
        node = destination.childNodes[0];
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
    var destination = document.getElementById(testElementId);
    destination.focus();
    document.execCommand("InsertText", false, testText);
    window.setTimeout(function() { verifySpellTest(10); }, 0);
}
