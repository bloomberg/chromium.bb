<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function doit()
{
    console.dir(window);
    runTest();
}

function test()
{
    InspectorTest.RuntimeAgent.evaluate("window", "console", false).then(evalCallback);

    function evalCallback(result)
    {
        if (!result) {
            testController.notifyDone("Exception");
            return;
        }
        if (result.type === "error")
            testController.notifyDone("Exception:" + result);
        var objectProxy = InspectorTest.runtimeModel.createRemoteObject(result);
        objectProxy.getOwnProperties(false, getPropertiesCallback);
    }

    function getPropertiesCallback(properties)
    {
        properties.sort(ObjectUI.ObjectPropertiesSection.CompareProperties);
        var golden = { "window": 1, "document": 1, "eval": 1, "console": 1, "frames": 1, "Array": 1, "doit": 1 }; 
        var result = {};
        for (var i = 0; i < properties.length; ++i) {
            var name = properties[i].name;
            if (golden[name])
                result[name] = 1;
        }
        InspectorTest.addObject(result);
        InspectorTest.completeTest();
    }
}

</script>
</head>

<body onload="doit()">
<p>
Tests that console dumps global object with properties.
</p>

</body>
</html>
