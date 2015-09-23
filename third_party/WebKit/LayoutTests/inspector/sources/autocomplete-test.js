function initialize_AutocompleteTest()
{

InspectorTest.dumpSuggestions = function(textEditor, lines) {
    var lineNumber = -1, columnNumber;
    for (var i = 0; i < lines.length; ++i) {
        var columnNumber = lines[i].indexOf("|");
        if (columnNumber !== -1) {
            lineNumber = i;
            break;
        }
    }
    if (lineNumber === -1)
        throw new Error("Test case is invalid: cursor position is not marked with '|' symbol.");
    textEditor.setText(lines.join("\n").replace("|", ""));
    textEditor.setSelection(WebInspector.TextRange.createFromLocation(lineNumber, columnNumber));
    var suggestions = [];
    InspectorTest.addSniffer(WebInspector.TextEditorAutocompleteController.prototype, "_onSuggestionsShownForTest", function (words) { suggestions = words; });
    textEditor._autocompleteController.autocomplete();

    InspectorTest.addResult("========= Selection In Editor =========");
    InspectorTest.dumpTextWithSelection(textEditor);
    InspectorTest.addResult("======= Autocomplete Suggestions =======");
    InspectorTest.addResult("[" + suggestions.join(", ") + "]");
}

}
