// A framework for testing.

var Framework = {};

Framework.safeRun = function(callback, onSuccess, onException, breakOnUncaught)
{
    try {
        callback();
        if (onSuccess)
            Framework.safeRun(onSuccess, undefined, onException, breakOnUncaught);
    } catch (e) {
        if (onException)
            Framework.safeRun(onException, undefined, breakOnUncaught ? Framework.breakInFramework : undefined);
        else if (breakOnUncaught)
            Framework.breakInFramework();
    }
}

Framework.throwFrameworkException = function(msg)
{
    throw Error("FrameworkException" + (msg ? ": " + msg : ""));
}

Framework.breakInFramework = function()
{
    debugger;
}

Framework.empty = function()
{
}

Framework.doSomeWork = function()
{
    const numberOfSteps = 50;
    for (var i = 0; i < numberOfSteps; ++i) {
        if (window["dummy property should not exist!" + i]) // Prevent optimizations.
            return i;
        Framework.safeRun(Framework.empty, Framework.empty, Framework.empty, true);
    }
}
