<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function templateString()
{
    console.log("The template string should not run and you should not see this log");
    return {
        shouldNotFindThis:56
    };
}

function shouldNotFindThisFunction() { }
function shouldFindThisFunction() { }
window["should not find this"] = true;

var myMap = new Map([['first', 1], ['second', 2], ['third', 3], ['shouldNotFindThis', 4]]);
var complicatedObject = {
    'foo-bar': true,
    '"double-qouted"': true,
    "'single-qouted'": true,
    "notDangerous();": true
}
function test()
{
    var consoleEditor;
    function testCompletions(text, expected, force)
    {
        var cursorPosition = text.indexOf('|');
        if (cursorPosition < 0)
            cursorPosition = Infinity;
        consoleEditor.setText(text.replace('|', ''));
        consoleEditor.setSelection(TextUtils.TextRange.createFromLocation(0, cursorPosition));
        consoleEditor._autocompleteController.autocomplete(force);
        return InspectorTest.addSnifferPromise(consoleEditor._autocompleteController, "_onSuggestionsShownForTest").then(checkExpected);

        function checkExpected(suggestions)
        {
            var completions = new Map(suggestions.map(suggestion => [suggestion.text, suggestion]));
            var message = "Checking '" + text.replace('\n', '\\n').replace('\r', '\\r') + "'";
            if (force)
                message += " forcefully";
            InspectorTest.addResult(message);
            for (var i = 0; i < expected.length; i++) {
                if (completions.has(expected[i])) {
                    if (completions.get(expected[i]).title)
                        InspectorTest.addResult("Found: " + expected[i] + ", displayed as " + completions.get(expected[i]).title);
                    else
                        InspectorTest.addResult("Found: " + expected[i]);
                } else {
                    InspectorTest.addResult("Not Found: " + expected[i]);
                }
            }
            InspectorTest.addResult("");
        }
    }
    function sequential(tests)
    {
        var promise = Promise.resolve();
        for (var i = 0; i < tests.length; i++)
            promise = promise.then(tests[i]);
        return promise;
    }

    sequential([
        InspectorTest.waitUntilConsoleEditorLoaded().then(e => consoleEditor = e),
        () => testCompletions("window.do", ["document"]),
        () => testCompletions("win", ["window"]),
        () => testCompletions('window["doc', ['"document"]']),
        () => testCompletions('window["document"].bo', ["body"]),
        () => testCompletions('window["document"]["body"].textC', ["textContent"]),
        () => testCompletions('document.body.inner', ["innerText", "innerHTML"]),
        () => testCompletions('document["body"][window.do', ["document"]),
        () => testCompletions('document["body"][window["document"].body.childNodes[0].text', ["textContent"]),
        () => testCompletions("templateString`asdf`should", ["shouldNotFindThis"]),
        () => testCompletions("window.document.BODY", ["body"]),
        () => testCompletions("window.dOcUmE", ["document"]),
        () => testCompletions("window.node", ["NodeList", "AudioNode", "GainNode"]),
        () => testCompletions("32", ["Float32Array", "Int32Array"]),
        () => testCompletions("window.32", ["Float32Array", "Int32Array"]),
        () => testCompletions("", ["window"], false),
        () => testCompletions("", ["window"], true),
        () => testCompletions('"string g', ["getComputedStyle"], false),
        () => testCompletions("`template string docu", ["document"], false),
        () => testCompletions("`${do", ["document"], false),
        () => testCompletions("// do", ["document"], false),
        () => testCompletions('["should', ["shouldNotFindThisFunction"]),
        () => testCompletions("shou", ["should not find this"]),
        () => testCompletions('myMap.get(', ['"first")', '"second")', '"third")']),
        () => testCompletions('myMap.get(\'', ['\'first\')', '\'second\')', '\'third\')']),
        () => testCompletions('myMap.set(\'firs', ['\'first\', ']),
        () => testCompletions('myMap.set(should', ['shouldFindThisFunction', 'shouldNotFindThis', '\"shouldNotFindThis\")']),
        () => testCompletions('myMap.delete(\'', ['\'first\')', '\'second\')', '\'third\')']),
        () => testCompletions("document.   bo", ["body"]),
        () => testCompletions("document.\tbo", ["body"]),
        () => testCompletions("document.\nbo", ["body"]),
        () => testCompletions("document.\r\nbo", ["body"]),
        () => testCompletions("document   [    'bo", ["'body']"]),
        () => testCompletions("function hey(should", ["shouldNotFindThisFunction"]),
        () => testCompletions("var should", ["shouldNotFindThisFunction"]),
        () => testCompletions("document[[win", ["window"]),
        () => testCompletions("document[   [win", ["window"]),
        () => testCompletions("document[   [  win", ["window"]),
        () => testCompletions('I|mag', ['Image', 'Infinity']),
        () => testCompletions('var x = (do|);', ['document']),
        () => testCompletions('complicatedObject["foo', ['"foo-bar"]']),
        () => testCompletions('complicatedObject["foo-', ['"foo-bar"]']),
        () => testCompletions('complicatedObject["foo-bar', ['"foo-bar"]']),
        () => testCompletions('complicatedObject["\'sing', ['"\'single-qouted\'"]']),
        () => testCompletions('complicatedObject[\'\\\'sing', ['\'\\\'single-qouted\\\'\']']),
        () => testCompletions('complicatedObject["\'single-qou', ['"\'single-qouted\'"]']),
        () => testCompletions('complicatedObject["\\"double-qouted\\"', ['"\\"double-qouted\\""]']),
        () => testCompletions('complicatedObject["notDangerous();', ['"notDangerous();"]']),
    ]).then(InspectorTest.completeTest);

}
</script>
</head>
<body onload="runTest()">
<p>Tests that console correctly finds suggestions in complicated cases.</p>
</body>
</html>
