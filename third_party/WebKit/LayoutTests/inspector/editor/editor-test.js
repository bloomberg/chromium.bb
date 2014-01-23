function initialize_EditorTests()
{

InspectorTest.createTestEditor = function(clientHeight, textEditorDelegate)
{
    loadScript("CodeMirrorTextEditor.js");
    var textEditor = new WebInspector.CodeMirrorTextEditor("", textEditorDelegate || new WebInspector.TextEditorDelegate());
    if (clientHeight)
        textEditor.element.style.height = clientHeight + "px";
    textEditor.show(WebInspector.inspectorView.element);
    return textEditor;
};

InspectorTest.textWithSelection = function(text, selection)
{
    if (!selection)
        return text;

    function lineWithCursor(line, column, cursorChar)
    {
        return line.substring(0, column) + cursorChar + line.substring(column);
    }

    var lines = text.split("\n");
    selection = selection.normalize();
    var endCursorChar = selection.isEmpty() ? "|" : "<";
    lines[selection.endLine] = lineWithCursor(lines[selection.endLine], selection.endColumn, endCursorChar);
    if (!selection.isEmpty()) {
        lines[selection.startLine] = lineWithCursor(lines[selection.startLine], selection.startColumn, ">");
    }
    return lines.join("\n");
}

InspectorTest.dumpTextWithSelection = function(textEditor, dumpWhiteSpaces)
{
    var text = InspectorTest.textWithSelection(textEditor.text(), textEditor.selection());
    if (dumpWhiteSpaces)
        text = text.replace(/ /g, ".");
    InspectorTest.addResult(text);
}

InspectorTest.typeIn = function(typeText)
{
    for(var charIndex = 0; charIndex < typeText.length; ++charIndex) {
        switch (typeText[charIndex]) {
        case "L":
            eventSender.keyDown("leftArrow");
            break;
        case "R":
            eventSender.keyDown("rightArrow");
            break;
        case "U":
            eventSender.keyDown("upArrow");
            break;
        case "D":
            eventSender.keyDown("downArrow");
            break;
        default:
            eventSender.keyDown(typeText[charIndex]);
        }
    }
}

InspectorTest.typeInEditor = function(editor, text)
{
    var selection = editor.selection();
    editor.editRange(editor.selection(), text);
    var lines = text.split("\n");
    var newSelection;
    if (lines.length > 1)
        newSelection = new WebInspector.TextRange.createFromLocation(selection.startLine + lines.length - 1, lines.pop().length);
    else
        newSelection = new WebInspector.TextRange.createFromLocation(selection.startLine, selection.endLine + lines[0].length);
    editor.setSelection(newSelection);
}

}
