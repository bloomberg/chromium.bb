<html>
<head>
<script src="../../../../http/tests/inspector/inspector-test.js"></script>
<script src="../../../../http/tests/inspector/debugger-test.js"></script>
<script>

function createPromise()
{
    var result = {};
    var p = new Promise(function(resolve, reject) {
        result.resolve = resolve;
        result.reject = reject;
    });
    result.promise = p;
    return result;
}

async function asyncCaught(promise) {
    try {
        await promise;
    } catch (e) { }
}

async function asyncUncaught(promise) {
    await promise;
}

function testFunction()
{
    var caught = createPromise();
    var uncaught = createPromise();

    asyncCaught(caught.promise);
    asyncUncaught(uncaught.promise);

    caught.reject(new Error("caught"));
    uncaught.reject(new Error("uncaught"));
}

var test = function()
{
    InspectorTest.setQuiet(true);
    InspectorTest.startDebuggerTest(step1);

    function waitUntilPausedNTimes(count, callback)
    {
        function inner()
        {
            if (count--)
                InspectorTest.waitUntilPausedAndDumpStackAndResume(inner);
            else
                callback();
        }
        inner();
    }

    function step1()
    {
        InspectorTest.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
        InspectorTest.showScriptSource("async-pause-on-exception.html", step2);
    }

    function step2()
    {
        InspectorTest.addResult("=== Pausing only on uncaught exceptions ===");
        InspectorTest.runTestFunction();
        waitUntilPausedNTimes(1, step3);
    }

    function step3()
    {
        InspectorTest.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
        InspectorTest.addResult("\n=== Pausing on all exceptions ===");
        InspectorTest.runTestFunction();
        waitUntilPausedNTimes(2, step4);
    }

    function step4()
    {
        InspectorTest.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions);
        InspectorTest.completeDebuggerTest();
    }
}

</script>
</head>

<body onload="runTest()">
<p>
Tests that pause on promise rejection works.
</p>

</body>
</html>
