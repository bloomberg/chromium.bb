description('Tests if the spellchecker behaves correctly when child has own '
    + 'spellcheck attribute.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var root = document.createElement("div");
document.body.appendChild(root);

function verifyChildSpellingMarker(element)
{
    var testElement = document.createElement("div");
    testElement.innerHTML = element.markup;
    root.appendChild(testElement);

    var childText = testElement.firstChild.childNodes[1].firstChild;
    document.getSelection().collapse(childText, 1);
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');

    if (window.internals) {
        debug(escapeHTML(testElement.innerHTML));

        shouldBecomeEqual("internals.hasSpellingMarker(document, 1, 2)", element.shouldBeMisspelled ? "true" : "false", function() {
            debug("");
            done();
        });
    } else
        done();
}

var testCases = [
    { markup: "<div contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", shouldBeMisspelled: false },
    { markup: "<div contentEditable>Foo <span id='child'>[]</span> Baz</div>", shouldBeMisspelled: true },
    { markup: "<div contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", shouldBeMisspelled: true },
    { markup: "<div spellcheck='false' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", shouldBeMisspelled: false },
    { markup: "<div spellcheck='false' contentEditable>Foo <span id='child'>[]</span> Baz</div>", shouldBeMisspelled: false },
    { markup: "<div spellcheck='false' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", shouldBeMisspelled: true },
    { markup: "<div spellcheck='true' contentEditable>Foo <span spellcheck='false' id='child'>[]</span> Baz</div>", shouldBeMisspelled: false },
    { markup: "<div spellcheck='true' contentEditable>Foo <span id='child'>[]</span> Baz</div>", shouldBeMisspelled: true },
    { markup: "<div spellcheck='true' contentEditable>Foo <span spellcheck='true' id='child'>[]</span> Baz</div>", shouldBeMisspelled: true }
];

function done()
{
    var nextTestCase = testCases.shift();
    if (nextTestCase)
        return setTimeout(verifyChildSpellingMarker(nextTestCase), 0);

    finishJSTest();
}
done();

var successfullyParsed = true;
