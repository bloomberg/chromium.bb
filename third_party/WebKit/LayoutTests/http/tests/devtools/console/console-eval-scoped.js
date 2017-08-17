<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

// Used to interfere into InjectedScript._propertyDescriptors()
Object.prototype.get = function() { return "FAIL"; };
Object.prototype.set = function() { return "FAIL"; };
Object.prototype.value = "FAIL";
Object.prototype.getter = "FAIL";
Object.prototype.setter = "FAIL";
Object.prototype.isOwn = true;
// Used to interfere into InjectedScript.primitiveTypes
Object.prototype.object = true;
// Used to interfere into InjectedScript.getEventListeners()
Object.prototype.nullValue = null;
Object.prototype.undefValue = undefined;

var foo = "bar";
var testObj = {
    get getter() { },
    set setter(_) { },
    baz: "baz"
};

function test()
{
    function snippet1() {
        (function (obj) {
           with (obj) {
              console.log('with: ' + a);
              eval("console.log('eval in with: ' + a)");
           }
        })({ a: "Object property value" })
    }

    function snippet2() {
        (function (a) { eval("console.log('eval in function: ' + a)"); })("Function parameter")
    }

    function bodyText(f) {
        var text = f.toString();
        var begin = text.indexOf("{");
        return text.substring(begin);
    }

    function dumpAndClearConsoleMessages(next)
    {
        InspectorTest.deprecatedRunAfterPendingDispatches(function() {
            InspectorTest.dumpConsoleMessages();
            Console.ConsoleView.clearConsole();
            InspectorTest.deprecatedRunAfterPendingDispatches(next);
        });
    }

    InspectorTest.runTestSuite([
        function testSnippet1(next)
        {
            InspectorTest.evaluateInPage(bodyText(snippet1), dumpAndClearConsoleMessages.bind(null, next));
        },

        function testSnippet2(next)
        {
            InspectorTest.evaluateInPage(bodyText(snippet2), dumpAndClearConsoleMessages.bind(null, next));
        },

        function testConsoleEvalPrimitiveValue(next)
        {
            InspectorTest.evaluateInConsole("foo", dumpAndClearConsoleMessages.bind(null, next));
        },

        async function testConsoleEvalObject(next)
        {
            var result = await InspectorTest.RuntimeAgent.evaluate("testObj");
            var properties = await InspectorTest.RuntimeAgent.getProperties(result.objectId, /* isOwnProperty */ true);
            for (var p of properties)
                InspectorTest.dump(p, { objectId: "formatAsTypeName", description: "formatAsDescription" });
            next();
        },

        function testGetEventListenersDoesNotThrow(next)
        {
            InspectorTest.evaluateInConsole("getEventListeners(document.body.firstChild)", dumpAndClearConsoleMessages.bind(null, next));
        }
    ]);
}
</script>
</head>
<body onload="runTest()">
<p>
Tests that evaluating 'console.log()' in the console will have access to its outer scope variables.
<a href="https://bugs.webkit.org/show_bug.cgi?id=60547">Bug 60547.</a>
</p>
</body>
</html>
