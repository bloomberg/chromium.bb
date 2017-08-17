<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>
var obj = {}
Object.defineProperty(obj, "foo", {enumerable: true, get: function() { return {a:1,b:2}; }});
Object.defineProperty(obj, "bar", {enumerable: false, set: function(x) { this.baz = x; }});

var arr = [];
Object.defineProperty(arr, 0, {enumerable: true, get: function() { return 1; }});
Object.defineProperty(arr, 1, {enumerable: false, set: function(x) { this.baz = x; }});

var myError = new Error("myError");
myError.stack = "custom stack";
var objWithGetterExceptions = {
    get error() { throw myError },
    get string() { throw "myString" },
    get number() { throw 123 },
    get function() { throw function() {} },
};

function logObject()
{
    console.log(obj);
    console.log(arr);
    console.log(objWithGetterExceptions);
}

function test()
{
    InspectorTest.evaluateInPage("logObject()", step2);
    function step2()
    {
        InspectorTest.dumpConsoleMessages();
        step3();
    }
    function step3()
    {
        InspectorTest.expandConsoleMessages(step4);
    }
    function step4()
    {
        InspectorTest.expandGettersInConsoleMessages(step5);
    }
    function step5()
    {
        InspectorTest.dumpConsoleMessages(false, true);
        InspectorTest.completeTest();
    }
}
</script>
</head>

<body onload="runTest()">
<p>
Tests that console logging dumps object values defined by getters and allows to expand it.
</p>
</body>
</html>