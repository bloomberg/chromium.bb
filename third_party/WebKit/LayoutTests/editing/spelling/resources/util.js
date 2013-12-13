function log(msg)
{
    document.getElementById("console").innerHTML += (msg + "\n");
}

function verifySpellTest(nretry, opt_doNotFinishTest)
{
    var node = window.destination;
    if (window.destination.childNodes.length > 0)
        node = window.destination.childNodes[0];
    if (nretry && !internals.markerCountForNode(node, "spelling")) {
        window.setTimeout(function() { verifySpellTest(nretry - 1, opt_doNotFinishTest); }, 0);
        return;
    }
    testFunctionCallback(node);
    if (!opt_doNotFinishTest)
        finishJSTest();
}

function initSpellTest(testElementId, testText, testFunction, opt_doNotFinishTest)
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
    window.setTimeout(function() { verifySpellTest(10, opt_doNotFinishTest); }, 0);
}

function findFirstTextNode(node)
{
    function iterToFindFirstTextNode(node)
    {
        if (node instanceof Text)
            return node;

        var childNodes = node.childNodes;
        for (var i = 0; i < childNodes.length; ++i) {
            var n = iterToFindFirstTextNode(childNodes[i]);
            if (n)
                return n;
        }

        return null;
    }

    if (node instanceof HTMLInputElement || node instanceof HTMLTextAreaElement) {
        return iterToFindFirstTextNode(internals.shadowRoot(node));
    } else {
        return iterToFindFirstTextNode(node);
    }
}

function typeText(elem, text)
{
    elem.focus();
    for (var i = 0; i < text.length; ++i) {
        typeCharacterCommand(text[i]);
    }
}
