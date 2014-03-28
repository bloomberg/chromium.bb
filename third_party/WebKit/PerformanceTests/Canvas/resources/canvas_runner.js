// CanvasRunner is a wrapper of PerformanceTests/resources/runner.js for canvas tests.
(function () {
    var MEASURE_DRAW_TIMES = 50;
    var MAX_MEASURE_DRAW_TIMES = 1000;
    var MAX_MEASURE_TIME_PER_FRAME = 1000; // 1 sec
    var currentTest = null;

    var CanvasRunner = {};

    CanvasRunner.start = function (test) {
        PerfTestRunner.prepareToMeasureValuesAsync({unit: 'runs/s',
            description: test.description});
        if (!test.doRun) {
            CanvasRunner.logFatalError("\ndoRun must be set.\n");
            return;
        }
        currentTest = test;
        runTest();
    }

    function runTest() {
        if (currentTest.preRun)
            currentTest.preRun();

        var start = PerfTestRunner.now();
        var count = 0;
        while ((PerfTestRunner.now() - start <= MAX_MEASURE_TIME_PER_FRAME) && (count * MEASURE_DRAW_TIMES < MAX_MEASURE_DRAW_TIMES)) {
            for (var i = 0; i < MEASURE_DRAW_TIMES; i++) {
                currentTest.doRun();
            }
            count++;
        }
        if (currentTest.ensureComplete)
            currentTest.ensureComplete();
        var elapsedTime = PerfTestRunner.now() - start;
        if (currentTest.postRun)
            currentTest.postRun();

        PerfTestRunner.measureValueAsync(MEASURE_DRAW_TIMES * count * 1000 / elapsedTime);

        requestAnimationFrame(runTest);
    }

    CanvasRunner.logFatalError = function (text) {
        PerfTestRunner.logFatalError(text);
    }

    window.CanvasRunner = CanvasRunner;
})();
