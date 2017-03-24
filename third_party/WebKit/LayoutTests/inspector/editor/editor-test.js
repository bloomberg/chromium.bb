function initialize_EditorTests()
{

InspectorTest.createTestEditor = function(clientHeight, textEditorDelegate)
{
    var textEditor = new SourceFrame.SourcesTextEditor(textEditorDelegate || new SourceFrame.SourcesTextEditorDelegate());
    clientHeight = clientHeight || 100;
    textEditor.element.style.height = clientHeight + "px";
    textEditor.element.style.flex = "none";
    textEditor.show(UI.inspectorView.element);
    return textEditor;
};

function textWithSelection(text, selections)
{
    if (!selections.length)
        return text;

    function lineWithCursor(line, column, cursorChar)
    {
        return line.substring(0, column) + cursorChar + line.substring(column);
    }

    var lines = text.split("\n");
    selections.sort(TextUtils.TextRange.comparator);
    for (var i = selections.length - 1; i >= 0; --i) {
        var selection = selections[i];
        selection = selection.normalize();
        var endCursorChar = selection.isEmpty() ? "|" : "<";
        lines[selection.endLine] = lineWithCursor(lines[selection.endLine], selection.endColumn, endCursorChar);
        if (!selection.isEmpty()) {
            lines[selection.startLine] = lineWithCursor(lines[selection.startLine], selection.startColumn, ">");
        }
    }
    return lines.join("\n");
}

InspectorTest.dumpTextWithSelection = function(textEditor, dumpWhiteSpaces)
{
    var text = textWithSelection(textEditor.text(), textEditor.selections());
    if (dumpWhiteSpaces)
        text = text.replace(/ /g, ".");
    InspectorTest.addResult(text);
}

InspectorTest.setLineSelections = function(editor, selections)
{
    var coords = [];
    for (var i = 0; i < selections.length; ++i) {
        var selection = selections[i];
        if (typeof selection.column === "number") {
            selection.from = selection.column;
            selection.to = selection.column;
        }
        coords.push(new TextUtils.TextRange(selection.line, selection.from, selection.line, selection.to));
    }
    editor.setSelections(coords);
}

InspectorTest.typeIn = function(editor, typeText, callback)
{
    callback = callback || new Function();
    var noop = new Function();
    for(var charIndex = 0; charIndex < typeText.length; ++charIndex) {
        // As soon as the last key event was processed, the whole text was processed.
        var iterationCallback = charIndex + 1 === typeText.length ? callback : noop;
        switch (typeText[charIndex]) {
        case "\n":
            InspectorTest.fakeKeyEvent(editor, "Enter", null, iterationCallback);
            break;
        case "L":
            InspectorTest.fakeKeyEvent(editor, "ArrowLeft", null, iterationCallback);
            break;
        case "R":
            InspectorTest.fakeKeyEvent(editor, "ArrowRight", null, iterationCallback);
            break;
        case "U":
            InspectorTest.fakeKeyEvent(editor, "ArrowUp", null, iterationCallback);
            break;
        case "D":
            InspectorTest.fakeKeyEvent(editor, "ArrowDown", null, iterationCallback);
            break;
        default:
            InspectorTest.fakeKeyEvent(editor, typeText[charIndex], null, iterationCallback);
        }
    }
}

var eventCodes = {
    Enter: 13,
    Home: 36,
    ArrowLeft: 37,
    ArrowUp: 38,
    ArrowRight: 39,
    ArrowDown: 40
};

function createCodeMirrorFakeEvent(editor, eventType, code, charCode, modifiers)
{
    function eventPreventDefault()
    {
        this._handled = true;
    }
    var event = {
        _handled: false,
        type: eventType,
        keyCode: code,
        charCode: charCode,
        preventDefault: eventPreventDefault,
        stopPropagation: function(){},
        target: editor._codeMirror.display.input.textarea
    };
    if (modifiers) {
        for (var i = 0; i < modifiers.length; ++i)
            event[modifiers[i]] = true;
    }
    return event;
}

function fakeCodeMirrorKeyEvent(editor, eventType, code, charCode, modifiers)
{
    var event = createCodeMirrorFakeEvent(editor, eventType, code, charCode, modifiers);
    switch(eventType) {
    case "keydown":
        editor._codeMirror.triggerOnKeyDown(event);
        break;
    case "keypress":
        editor._codeMirror.triggerOnKeyPress(event);
        break;
    case "keyup":
        editor._codeMirror.triggerOnKeyUp(event);
        break;
    default:
        throw new Error("Unknown KeyEvent type");
    }
    return event._handled;
}

function fakeCodeMirrorInputEvent(editor, character)
{
    if (typeof character === "string")
        editor._codeMirror.display.input.textarea.value += character;
}

InspectorTest.fakeKeyEvent = function(editor, originalCode, modifiers, callback)
{
    modifiers = modifiers || [];
    var code;
    var charCode;
    if (originalCode === "'") {
        code = 222;
        charCode = 0;
    } else if (originalCode === "\"") {
        code = 222;
        modifiers.push("shiftKey");
        charCode = 34;
    } else if (originalCode === "(") {
        code = "9".charCodeAt(0);
        modifiers.push("shiftKey");
        charCode = originalCode.charCodeAt(0);
    }
    var code = code || eventCodes[originalCode] || originalCode;
    if (typeof code === "string")
        code = code.charCodeAt(0);
    if (fakeCodeMirrorKeyEvent(editor, "keydown", code, charCode, modifiers)) {
        callback();
        return;
    }
    if (fakeCodeMirrorKeyEvent(editor, "keypress", code, charCode, modifiers)) {
        callback();
        return;
    }
    fakeCodeMirrorInputEvent(editor, originalCode);
    fakeCodeMirrorKeyEvent(editor, "keyup", code, charCode, modifiers);

    function callbackWrapper()
    {
        editor._codeMirror.off("inputRead", callbackWrapper);
        callback();
    }
    editor._codeMirror.on("inputRead", callbackWrapper);
}

InspectorTest.dumpSelectionStats = function(textEditor)
{
    var listHashMap = {};
    var sortedKeys = [];
    var selections = textEditor.selections();
    for (var i = 0; i < selections.length; ++i) {
        var selection = selections[i];
        var text = textEditor.text(selection);
        if (!listHashMap[text]) {
            listHashMap[text] = 1;
            sortedKeys.push(text);
        } else {
            ++listHashMap[text];
        }
    }
    for (var i = 0; i < sortedKeys.length; ++i) {
        var keyName = sortedKeys[i];
        if (!keyName.length)
            keyName = "<Empty string>";
        else
            keyName = "'" + keyName + "'";
        InspectorTest.addResult(keyName + ": " + listHashMap[sortedKeys[i]]);
    }
}

}
