<?php
    header("Link: <http://127.0.0.1:8000/resources/dummy.js>;rel=preload;as=script", false);
?>
<html>
<head>
<script src="../inspector-test.js"></script>
<script src="../network-test.js"></script>
<style>
    body { overflow: hidden; }
</style>
<script>
function scheduleScriptLoad() {
    window.setTimeout(loadScript, 0);
}

function loadScript() {
    var script = document.createElement("script");
    script.type = "text/javascript";
    script.src = "http://127.0.0.1:8000/resources/dummy.js";
    script.onload = function() {
        setTimeout(scriptLoaded, 0);
    };
    document.head.appendChild(script);
}

function scriptLoaded() {
    var resources = performance.getEntriesByType("resource");
    var dummies = 0;
    for (var i = 0; i < resources.length; ++i) {
        if (resources[i].name.indexOf("dummy.js") != -1)
            ++dummies;
    }

    var text;
    if (dummies == 1)
        text = document.createTextNode("PASS - 1 resource loaded");
    else
        text = document.createTextNode("FAIL - " + dummies + " resources loaded");
    var result = document.createElement("p");
    result.appendChild(text);
    document.body.appendChild(result);
    console.log("done");
}

function test()
{
    InspectorTest.NetworkAgent.setCacheDisabled(true, step1);

    function step1()
    {
        InspectorTest.reloadPage(step2);
    }

    function step2(msg)
    {
        InspectorTest.addConsoleSniffer(done);
        InspectorTest.evaluateInPage("scheduleScriptLoad()");
    }

    function done(msg)
    {
        InspectorTest.completeTest();
    }
}
</script>
</head>
<body onload="runTest(true)">
    <p>Tests disabling cache from inspector and seeing that preloads are not evicted from memory cache.</p>
</body>
</html>

