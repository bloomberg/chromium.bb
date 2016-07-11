function test()
{
    InspectorTest.sendCommand("Debugger.enable", {});
    InspectorTest.sendCommand("DOM.enable", {});
    InspectorTest.sendCommand("DOMDebugger.enable", {});
    InspectorTest.sendCommand("DOMDebugger.setInstrumentationBreakpoint", {"eventName":"scriptBlockedByCSP"});
    InspectorTest.eventHandler["Debugger.paused"] = handleDebuggerPaused;

    var expressions = [
        "\n document.getElementById('testButton').click();",

        "\n var script = document.createElement('script');" +
        "\n script.innerText = 'alert(1)';" +
        "\n document.body.appendChild(script);",

        "\n var a = document.createElement('a');" +
        "\n a.setAttribute('href', 'javascript:alert(1);');" +
        "\n var dummy = 1; " +
        "\n document.body.appendChild(a); a.click();"
    ];
    var descriptions = [
        "blockedEventHandler",
        "blockedScriptInjection",
        "blockedScriptUrl"
    ];

    function nextExpression()
    {
        if (!expressions.length) {
            InspectorTest.completeTest();
            return;
        }
        var description = descriptions.shift();
        InspectorTest.log("\n-------\n" + description);
        InspectorTest.sendCommand("Runtime.evaluate", { "expression": "function " + description + "() {" + expressions.shift() + "}\n" + description + "()"});
    }

    function handleDebuggerPaused(messageObject)
    {
        var params = messageObject.params;
        InspectorTest.log("Paused at: " + params.callFrames[0].functionName + "@" + params.callFrames[0].location.lineNumber);
        InspectorTest.log("Reason: " + params.reason + "; Data:");
        InspectorTest.logObject(params.data);
        InspectorTest.sendCommand("Debugger.resume", { }, nextExpression);
    }

    nextExpression();
}

window.addEventListener("load", runTest.bind(null, false));
