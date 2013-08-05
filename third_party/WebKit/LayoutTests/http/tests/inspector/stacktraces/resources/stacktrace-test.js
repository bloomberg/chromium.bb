function test() {
    InspectorTest.addConsoleSniffer(addMessage);

    function addMessage() {
        for (var i = 0; i < WebInspector.console.messages.length; ++i) {
            var message = WebInspector.console.messages[i];
            if (!WebInspector.console.filter.shouldBeVisible(message))
                continue;
            InspectorTest.addResult("Message[" + i + "]: " + WebInspector.displayNameForURL(message.url) + ":" + message.line + " " + message.message);
            var trace = message.stackTrace;
            if (!trace) {
                InspectorTest.addResult("FAIL: no stack trace attached to message #" + i);
            } else {
                InspectorTest.addResult("Stack Trace:\n");
                InspectorTest.addResult("    url: " + trace[0].url);
                InspectorTest.addResult("    function: " + trace[0].functionName);
                InspectorTest.addResult("    line: " + trace[0].lineNumber);
            }
        }
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateInPage("thisTest()");
}
