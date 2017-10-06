TestRunner.addResult("This tests if the TextPrompt autocomplete works properly.");

var suggestions = ["heyoo", "hey it's a suggestion", "hey another suggestion"].map(s => ({text: s}));
var prompt = new UI.TextPrompt();
prompt.initialize(() => Promise.resolve(suggestions));
var div = document.createElement("div");
UI.inspectorView.element.appendChild(div);
prompt.attachAndStartEditing(div);
prompt.setText("hey");
TestRunner.addSnifferPromise(prompt, "_completionsReady").then(step2);
prompt.complete();
function step2() {
    TestRunner.addResult("Text:" + prompt.text());
    TestRunner.addResult("TextWithCurrentSuggestion:" + prompt.textWithCurrentSuggestion());

    TestRunner.addResult("Test with inexact match:");
    prompt.clearAutocomplete();
    prompt.setText("inexactmatch");
    TestRunner.addSnifferPromise(prompt, "_completionsReady").then(step3);
    prompt.complete();
}
function step3() {
    TestRunner.addResult("Text:" + prompt.text());
    TestRunner.addResult("TextWithCurrentSuggestion:" + prompt.textWithCurrentSuggestion());
    TestRunner.completeTest();
}