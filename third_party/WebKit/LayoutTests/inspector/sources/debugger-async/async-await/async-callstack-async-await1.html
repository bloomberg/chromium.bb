<html>
<head>
<script src="../../../../http/tests/inspector/inspector-test.js"></script>
<script src="../../../../http/tests/inspector/debugger-test.js"></script>
<script>

function timeoutPromise(value, ms)
{
    return new Promise(function promiseCallback(resolve, reject) {
        function resolvePromise()
        {
            resolve(value);
        }
        function rejectPromise()
        {
            reject(value);
        }
        if (value instanceof Error)
            setTimeout(rejectPromise, ms || 0);
        else
            setTimeout(resolvePromise, ms || 0);
    });
}

function settledPromise(value)
{
    function resolveCallback(resolve, reject)
    {
        resolve(value);
    }
    function rejectCallback(resolve, reject)
    {
        reject(value);
    }
    if (value instanceof Error)
        return new Promise(rejectCallback);
    else
        return new Promise(resolveCallback);
}

function testFunction()
{
    setTimeout(testFunctionTimeout, 0);
}

function testFunctionTimeout()
{
    var functions = [doTestPromiseConstructor, doTestSettledPromisesResolved, doTestSettledPromisesRejected];
    for (var i = 0; i < functions.length; ++i)
        functions[i]();
}

function thenCallback(value)
{
    debugger;
}

function errorCallback(error)
{
    debugger;
}

async function doTestPromiseConstructor()
{
    try {
        let result = await new Promise(function promiseCallback(resolve, reject) {
            resolve(1);
            debugger;
        });
        thenCallback(result);
    } catch (e) {
        errorCallback(e);
    }
}

async function doTestSettledPromisesResolved()
{
    try {
        let value = await settledPromise("resolved");
        thenCallback(value);
    } catch (e) {
        errorCallback(e);
    }
}

async function doTestSettledPromisesRejected()
{
    try {
        let value = await settledPromise(new Error("rejected"));
        thenCallback(value);
    } catch (e) {
        errorCallback(e);
    }
}

var test = function()
{
    var totalDebuggerStatements = 4;
    InspectorTest.runAsyncCallStacksTest(totalDebuggerStatements);
}

</script>
</head>

<body onload="runTest()">
<p>
Tests asynchronous call stacks for async functions.
</p>

</body>
</html>
