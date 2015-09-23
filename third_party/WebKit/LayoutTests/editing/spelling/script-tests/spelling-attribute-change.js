description('Tests if the spellchecker behaves correctly when the spellcheck attribute '
    + 'is being changed by the script.');

jsTestIsAsync = true;

if (window.internals) {
    internals.settings.setUnifiedTextCheckerEnabled(true);
    internals.settings.setAsynchronousSpellCheckingEnabled(true);
}

var parent = document.createElement("div");
document.body.appendChild(parent);
var sel = document.getSelection();

function testSpellCheckingEnabled(target, enabled)
{
    target.spellcheck = enabled;

    if (target.tagName == "SPAN") {
        target.appendChild(document.createTextNode("Hello,"));
        sel.setBaseAndExtent(target, 6, target, 6);
    } else if (target.tagName == "INPUT" || target.tagName == "TEXTAREA") {
        target.focus();
        document.execCommand("InsertText", false, "Hello,");
    }

    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, 'z');
    document.execCommand("InsertText", false, ' ');

    window.target = target;
    shouldBe("target.spellcheck", enabled ? "true" : "false");
    if (window.internals)
        shouldBecomeEqual("internals.hasSpellingMarker(document, 6, 2)", enabled ? "true" : "false", done);
    else
        done();
}

function createElement(tagName, spellcheck)
{
    var target = document.createElement(tagName);
    if (tagName == "SPAN")
        target.setAttribute("contentEditable", "true");
    if (spellcheck)
        target.setAttribute("spellcheck", spellcheck);
    return target;
}

function testFor(tagName, spellCheckAttribueValues)
{
    var target = createElement(tagName, spellCheckAttribueValues.initialValue);
    parent.appendChild(target);

    testSpellCheckingEnabled(target, spellCheckAttribueValues.destinationValue);
}

var testElements = [
    "SPAN",
    "INPUT",
    "TEXTAREA"
];

const spellcheckAttributeVariances = [
   { initialValue: undefined, destinationValue: true },
   { initialValue: undefined, destinationValue: false },
   { initialValue: true, destinationValue: true },
   { initialValue: true, destinationValue: false },
   { initialValue: false, destinationValue: true },
   { initialValue: false, destinationValue: false }
];

var iterator = 0;
var currentElement = null;

function done()
{
    if (!currentElement) {
        currentElement = testElements.shift();
        // All elements have been already taken.
        if (!currentElement)
            return finishJSTest();
    }

    if (iterator != spellcheckAttributeVariances.length)
        setTimeout(testFor(currentElement, spellcheckAttributeVariances[iterator++]), 0);
    else {
        iterator = 0;
        currentElement = null;
        done();
    }
}
done();

var successfullyParsed = true;
