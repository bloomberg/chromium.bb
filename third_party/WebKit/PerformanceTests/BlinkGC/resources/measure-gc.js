if (!window.PerfTestRunner)
    console.log("measure-gc.js requires PerformanceTests/resources/runner.js to be loaded.");
if (!window.internals)
    console.log("measure-gc.js requires window.internals.");
if (!window.GCController && !window.gc)
    console.log("measure-gc.js requires GCController or exposed gc().");

(function (PerfTestRunner) {
    PerfTestRunner.measureBlinkGCTime = function(test) {
        if (!test.unit)
            test.unit = 'ms';
        if (!test.warmUpCount)
            test.warmUpCount = 3;
        PerfTestRunner.prepareToMeasureValuesAsync(test);

        // Force a V8 GC before running Blink GC test to avoid measuring marking from stale V8 wrappers.
        if (window.GCController)
            GCController.collectAll();
        else if (window.gc) {
            for (var i = 0; i < 7; i++)
                gc();
        }
        setTimeout(runTest, 0);
    }

    var NumberOfGCRunsPerForceBlinkGC = 5;

    function runTest() {
        // forceBlinkGCWithoutV8GC will schedule 5 Blink GCs at the end of event loop.
        // setTimeout function runs on next event loop, so assuming that event loop is not busy,
        // we can estimate GC time by measuring the delay of setTimeout function.
        internals.forceBlinkGCWithoutV8GC();
        var start = PerfTestRunner.now();
        setTimeout(function() {
            var end = PerfTestRunner.now();
            PerfTestRunner.measureValueAsync((end - start) / NumberOfGCRunsPerForceBlinkGC);
            setTimeout(runTest, 0);
        }, 0);
    }
})(window.PerfTestRunner);
