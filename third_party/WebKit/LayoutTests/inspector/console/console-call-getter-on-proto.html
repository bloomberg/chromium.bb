<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
function logObject()
{
    var A = function() { this.value = 239; }
    A.prototype = {
        constructor: A,
        get foo()
        {
            return this.value;
        }
    }
    var B = function() { A.call(this); }
    B.prototype = {
        constructor: B,
        __proto__: A.prototype
    }
    console.log(new B());
}

function test()
{
    InspectorTest.evaluateInPage("logObject()", step2);
    function step2()
    {
        InspectorTest.expandConsoleMessages(step3);
    }
    function expandTreeElementFilter(treeElement)
    {
        var name = treeElement.nameElement && treeElement.nameElement.textContent;
        return name === "__proto__";
    }
    function step3()
    {
        InspectorTest.expandConsoleMessages(step4, expandTreeElementFilter);
    }
    function step4()
    {
        InspectorTest.expandGettersInConsoleMessages(step5);
    }
    function step5()
    {
        InspectorTest.dumpConsoleMessages(false, false, InspectorTest.textContentWithLineBreaks);
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="runTest()">
<p>
Tests that calling getter on prototype will call it on the object.
</p>
</body>
</html>
