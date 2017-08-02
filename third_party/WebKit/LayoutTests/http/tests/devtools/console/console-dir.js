<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>

<script>
function onload()
{
    console.dir(["test1", "test2"]);
    console.dir(document.childNodes);
    console.dir(document.evaluate("//head", document, null, XPathResult.ANY_TYPE, null));

    // Object with properties containing whitespaces
    var obj = { $foo5_: 0 };
    obj[" a b "] = " a b ";
    obj["c d"] = "c d";
    obj[""] = "";
    obj["  "] = "  ";
    obj["a\n\nb\nc"] = "a\n\nb\nc";
    obj["negZero"] = -0;
    console.dir(obj);

    // This should correctly display information about the function.
    console.dir(function() {});

    // Test function inferred name in prototype constructor.
    var outer = { inner: function() {} };
    console.dir(new outer.inner());

    // Test "No Properties" placeholder.
    console.dir({ __proto__: null });
    console.dir({ foo: { __proto__: null }});
    // Test "No Scopes" placeholder.
    console.dir(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get);

    // Test big typed array: should be no crash or timeout.
    var bigTypedArray = new Uint8Array(new ArrayBuffer(400 * 1000 * 1000));
    bigTypedArray["FAIL"] = "FAIL: Object.getOwnPropertyNames() should not have been run";
    console.dir(bigTypedArray);

    // document.createEvent("Event") has a special property "isTrusted" flagged "Unforgeable".
    var event = document.createEvent("Event");
    Object.defineProperty(event, "timeStamp", {value: 0})
    console.dir(event);

    runTest();
}
//# sourceURL=console-dir.html
</script>

<script>
function test()
{
    InspectorTest.expandConsoleMessages(step1, expandTreeElementFilter);

    function expandTreeElementFilter(treeElement)
    {
        var name = treeElement.nameElement && treeElement.nameElement.textContent;
        return name === "foo" || treeElement.title === "<function scope>";
    }

    function step1()
    {
        InspectorTest.expandConsoleMessages(dumpConsoleMessages, expandTreeElementFilter);
    }

    function dumpConsoleMessages()
    {
        InspectorTest.dumpConsoleMessagesIgnoreErrorStackFrames();
        InspectorTest.completeTest();
    }
}

</script>
</head>

<body onload="onload()">
<p>
Tests that console logging dumps proper messages.
</p>

</body>
</html>
