var initialize_LiveEditTest = function() {

InspectorTest.replaceInSource = function(sourceFrame, string, replacement)
{
    sourceFrame._textEditor.setReadOnly(false);
    for (var i = 0; i < sourceFrame._textEditor.linesCount; ++i) {
        var line = sourceFrame._textEditor.line(i);
        var column = line.indexOf(string);
        if (column === -1)
            continue;
        range = new Common.TextRange(i, column, i, column + string.length);
        var newRange = sourceFrame._textEditor.editRange(range, replacement);
        break;
    }
}

InspectorTest.commitSource = function(sourceFrame)
{
    sourceFrame.commitEditing();
}

InspectorTest.undoSourceEditing = function(sourceFrame)
{
    sourceFrame._textEditor.undo();
}

};
