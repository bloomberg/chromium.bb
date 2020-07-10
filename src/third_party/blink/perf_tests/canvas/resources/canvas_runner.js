/*
Runs canvas performance tests to calculate runs/sec for test.doRun().
This module works in two different ways, depending on requestAnimationFrame
(RAF). The query string `RAF` in the url determines which test is run.
*/
(function () {
  var MEASURE_DRAW_TIMES = 50;
  var MAX_MEASURE_DRAW_TIMES = 1000;
  var MAX_MEASURE_TIME_PER_FRAME = 1000; // 1 sec
  var IS_RAF_TEST = (
    document.location.search.substr(1,).toUpperCase() === "RAF");
  var currentTest = null;
  var isTestDone = false;

  var CanvasRunner = {};

  CanvasRunner.start = function (test) {
    PerfTestRunner.startMeasureValuesAsync({
      unit: 'runs/s',
      description: test.description,
      done: testDone,
      run: function() {
        if (!test.doRun) {
          CanvasRunner.logFatalError("doRun must be set.");
          return;
        }
        currentTest = test;
        if (IS_RAF_TEST === true) {
          runTestRAF();
        } else {
          runTest();
        }
      }});
  }

  // Times the CPU on the main thread
  function runTest() {
    try {
      if (currentTest.preRun)
        currentTest.preRun();

      var start = PerfTestRunner.now();
      var count = 0;
      while ((PerfTestRunner.now() - start <= MAX_MEASURE_TIME_PER_FRAME) &&
        (count * MEASURE_DRAW_TIMES < MAX_MEASURE_DRAW_TIMES)) {
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

      let runsPerSecond = MEASURE_DRAW_TIMES * count * 1000 / elapsedTime;
      PerfTestRunner.measureValueAsync(runsPerSecond);
    } catch(err) {
      CanvasRunner.logFatalError("test fails due to GPU issue. " + err);
      throw err;
    }

    if (!isTestDone)
      requestAnimationFrame(runTest);
  }

  // Times CPU + raster + GPU for draw calls, invoked with the ?RAF query string
  // All times in milliseconds
  function runTestRAF() {
    // How long in ms we want each trial to take
    // Must be much greater than 16 (16ms is the v-sync rate)
    const GOAL_TIME = 200;

    function runTrial(numRuns) {
      if (currentTest.preRun) currentTest.preRun();
      let startTime = PerfTestRunner.now();
      for (var i = 0; i < numRuns; i++) {
        currentTest.doRun();
      }
      requestAnimationFrame(() => {
        let elapsedTime = PerfTestRunner.now() - startTime;
        let runsPerSecond = numRuns * 1000 / elapsedTime;
        PerfTestRunner.measureValueAsync(runsPerSecond);
        if (!isTestDone) runTrial(numRuns, startTime);
      });
      if (currentTest.ensureComplete) currentTest.ensureComplete();
      if (currentTest.postRun) currentTest.postRun();
    }

    // Figure out how many times currentTest.doRun() + RAF will be required
    // to last GOAL_TIME
    function calculateNumberOfRuns(resolve, numRuns) {
      numRuns = numRuns || 1;
      if (currentTest.preRun) currentTest.preRun();
      const startTime = PerfTestRunner.now();

      for (var i = 0; i < numRuns; i++) {
        currentTest.doRun();
      }

      requestAnimationFrame(() => {
        let elapsedTime = PerfTestRunner.now() - startTime;
        if (elapsedTime >= GOAL_TIME) {
          const timePerRun = elapsedTime / numRuns;
          const numRunsFinal = Math.round(GOAL_TIME / timePerRun);
          if (currentTest.ensureComplete) currentTest.ensureComplete();
          if (currentTest.postRun) currentTest.postRun();
          resolve(numRunsFinal);
        } else {
          calculateNumberOfRuns(resolve, numRuns * 2);
        }
      });
    }

    try {
      new Promise(function(resolve, reject) {
        calculateNumberOfRuns(resolve);
      }).then(function(numberOfRuns) {
        runTrial(numberOfRuns);
      });
    } catch(err) {
      CanvasRunner.logFatalError("test fails due to GPU issue. " + err);
      throw err;
    }
  }

  function testDone() {
    isTestDone = true;
  }

  CanvasRunner.logFatalError = function (text) {
    PerfTestRunner.logFatalError(text);
  }

  CanvasRunner.startPlayingAndWaitForVideo = function (video, callback) {
    var gotPlaying = false;
    var gotTimeUpdate = false;

    var maybeCallCallback = function() {
      if (gotPlaying && gotTimeUpdate && callback) {
        callback(video);
        callback = undefined;
        video.removeEventListener('playing', playingListener, true);
        video.removeEventListener('timeupdate', timeupdateListener, true);
      }
    };

    var playingListener = function() {
      gotPlaying = true;
      maybeCallCallback();
    };

    var timeupdateListener = function() {
      // Checking to make sure the current time has advanced beyond
      // the start time seems to be a reliable heuristic that the
      // video element has data that can be consumed.
      if (video.currentTime > 0.0) {
        gotTimeUpdate = true;
        maybeCallCallback();
      }
    };

    video.addEventListener('playing', playingListener, true);
    video.addEventListener('timeupdate', timeupdateListener, true);
    video.loop = true;
    video.play();
  }

  window.CanvasRunner = CanvasRunner;
})();
