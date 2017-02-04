function initialize_AutocompleteTest()
{

    InspectorTest.dumpSuggestions = function(textEditor, lines)
    {
        var resolve;
        var promise = new Promise(fulfill => resolve = fulfill);
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
        textEditor.setSelection(Common.TextRange.createFromLocation(lineNumber, columnNumber));
        InspectorTest.addSniffer(TextEditor.TextEditorAutocompleteController.prototype, "_onSuggestionsShownForTest", suggestionsShown);
        textEditor._autocompleteController.autocomplete();
        function suggestionsShown(words)
        {
            InspectorTest.addResult("========= Selection In Editor =========");
            InspectorTest.dumpTextWithSelection(textEditor);
            InspectorTest.addResult("======= Autocomplete Suggestions =======");
            InspectorTest.addResult("[" + words.map(item => item.text).join(", ") + "]");
            resolve();
        }
        return promise;
    }

}
