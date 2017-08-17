<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function registerNonElement()
{
    var nonElementProto = {
        createdCallback: function()
        {
            console.dir(this);
        }
    };
    var nonElementOptions = { prototype: nonElementProto };
    document.registerElement("foo-bar", nonElementOptions);
}

function registerElement()
{
    var elementProto = Object.create(HTMLElement.prototype);
    elementProto.createdCallback = function()
    {
        console.dir(this);
    };
    var elementOptions = { prototype: elementProto };
    document.registerElement("foo-bar2", elementOptions);
}

function test()
{
    InspectorTest.waitUntilMessageReceived(step1);
    InspectorTest.evaluateInPage("registerNonElement();");

    function step1()
    {
        InspectorTest.waitUntilMessageReceived(step2);
        InspectorTest.evaluateInPage("registerElement();");
    }

    function step2()
    {
        InspectorTest.dumpConsoleMessages();
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="runTest()">
<foo-bar></foo-bar>
<foo-bar2></foo-bar2>
<p>
Tests that logging custom elements uses proper formatting.
</p>

</body>
</html>
