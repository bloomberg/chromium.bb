description('Spelling markers should remain while merging two lines.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var testRoot = document.createElement("div");
document.body.insertBefore(testRoot, document.body.firstChild);

function setup(targetName)
{
    var div = document.createElement("div");
    div.id = targetName;
    div.contentEditable = true;
    div.innerHTML = "<div>OK</div><div>OK zz OK</div>";
    testRoot.appendChild(div);

    div.focus();
    return document.getSelection();
}

function firstLineText(targetName)
{
    var div = document.getElementById(targetName);
    return div.firstChild.innerText.trim();
}

function testTwoLinesMisspellings()
{
    window.sel = setup("two-lines"); // ^OK

    sel.modify("move", "forward", "line"); // ^OK zz OK
    for (var i = 0; i < 3; i++)
        sel.modify("move", "forward", "word");

    shouldBeEqualToString("firstLineText('two-lines')", "OK");
    shouldBeEqualToString("sel.anchorNode.data", "OK zz OK");
    if (window.internals)
        shouldBecomeEqual("internals.hasSpellingMarker(document, 3, 2)", "true", done);
    else
        done();
}

function testMisspellingsAfterLineMergeUsingDelete()
{
    window.sel = setup("merged-lines-delete-command"); // ^OK

    sel.modify("move", "forward", "line"); // ^OK zz OK
    document.execCommand("Delete", false); // OK^OK zz OK
    sel.modify("move", "right", "line"); // OKOK zz OK^

    shouldBeEqualToString("firstLineText('merged-lines-delete-command')", "OKOK zz OK");
    shouldBeEqualToString("sel.anchorNode.data", "OKOK zz OK");
    if (window.internals)
        shouldBecomeEqual("internals.hasSpellingMarker(document, 5, 2)", "true", done);
    else
        done();
}

function testMisspellingsAfterLineMergeUsingForwardDelete()
{
    window.sel = setup("merged-lines-forward-delete-command"); // ^OK

    sel.modify("move", "forward", "character"); // O^K
    sel.modify("move", "forward", "character"); // OK^
    document.execCommand("ForwardDelete", false); // OK^OK zz OK
    sel.modify("move", "right", "line"); // OKOK zz OK^

    shouldBeEqualToString("firstLineText('merged-lines-forward-delete-command')", "OKOK zz OK");
    shouldBeEqualToString("sel.anchorNode.data", "OKOK zz OK");
    if (window.internals)
        shouldBecomeEqual("internals.hasSpellingMarker(document, 5, 2)", "true", done);
    else
        done();
}

var tests = [
    function() { testTwoLinesMisspellings(); },
    function() { testMisspellingsAfterLineMergeUsingDelete(); },
    function() { testMisspellingsAfterLineMergeUsingForwardDelete(); }
];

function done()
{
    var next = tests.shift();
    if (next)
        return setTimeout(next, 0);

    if (window.internals)
        testRoot.style.display = "none";

    finishJSTest();
}
done();

var successfullyParsed = true;
