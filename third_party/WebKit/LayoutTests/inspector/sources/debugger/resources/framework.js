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

Framework.schedule = function(callback, delay)
{
    setTimeout(callback, delay || 0);
}

Framework.willSchedule = function(callback, delay)
{
    return function Framework_willSchedule() {
        return Framework.schedule(callback, delay);
    };
}

Framework.doSomeAsyncChainCalls = function(callback)
{
    var func1 = Framework.willSchedule(function Framework_inner1() {
        if (callback)
            callback();
    });
    var func2 = Framework.willSchedule(function Framework_inner2() {
        if (window.callbackFromFramework)
            window.callbackFromFramework(func1);
        else
            func1();
    });
    Framework.schedule(func2);
}
