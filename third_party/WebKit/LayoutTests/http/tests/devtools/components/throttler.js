// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies throttler behavior.\n`);

  class TimeoutMock {
    constructor() {
      this._timeoutId = 0;
      this._timeoutIdToProcess = {};
      this._timeoutIdToMillis = {};
      this.setTimeout = this.setTimeout.bind(this);
      this.clearTimeout = this.clearTimeout.bind(this);
    }

    /**
         * @param {!Function} operation
         * @param {number} timeout
         */
    setTimeout(operation, timeout) {
      this._timeoutIdToProcess[++this._timeoutId] = operation;
      this._timeoutIdToMillis[this._timeoutId] = timeout;
      return this._timeoutId;
    }

    /**
            *
            * @param {number} timeoutId
            */
    clearTimeout(timeoutId) {
      delete this._timeoutIdToProcess[timeoutId];
      delete this._timeoutIdToMillis[timeoutId];
    }

    /**
            * @return {!Array<number>}
            */
    activeTimersTimeouts() {
      return Object.values(this._timeoutIdToMillis);
    }

    fireAllTimers() {
      for (const timeoutId in this._timeoutIdToProcess)
        this._timeoutIdToProcess[timeoutId].call(window);
      this._timeoutIdToProcess = {};
      this._timeoutIdToMillis = {};
    }
  }

  var ProcessMock = function(name, runnable) {
    this._runnable = runnable;
    this.processName = name;
    this.run = this.run.bind(this);
    this.run.processName = name;

    this.startPromise = new Promise(onStart.bind(this));
    this.finishPromise = new Promise(onFinish.bind(this));

    function onStart(startCallback) {
      this._startCallback = startCallback;
    }

    function onFinish(finishCallback) {
      this._finishCallback = finishCallback;
    }
  };

  ProcessMock.create = function(name, runnable) {
    return new ProcessMock(name, runnable);
  };

  ProcessMock.prototype = {
    run: function() {
      TestRunner.addResult('Process \'' + this.processName + '\' STARTED.');
      this._startCallback();
      if (this._runnable)
        this._runnable.call(null);
      return this.finishPromise;
    },

    finish: function() {
      this.startPromise.then(onFinish.bind(this));

      function onFinish() {
        TestRunner.addResult('Process \'' + this.processName + '\' FINISHED.');
        this._finishCallback();
      }
    },
  };

  var throttler = new Common.Throttler(1989);
  var timeoutMock = new TimeoutMock();
  throttler._setTimeout = timeoutMock.setTimeout;
  throttler._clearTimeout = timeoutMock.clearTimeout;
  TestRunner.addSniffer(throttler, 'schedule', logSchedule, true);

  function testSimpleSchedule(next, runningProcess) {
    assertThrottlerIdle();
    throttler.schedule(ProcessMock.create('operation #1').run, false);
    var process = ProcessMock.create('operation #2');
    throttler.schedule(process.run);

    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }

    promise
        .then(function() {
          assertThrottlerTimeout();
          timeoutMock.fireAllTimers();
          process.finish();
          return waitForProcessFinish();
        })
        .then(next);
  }

  function testAsSoonAsPossibleOverrideTimeout(next, runningProcess) {
    assertThrottlerIdle();
    throttler.schedule(ProcessMock.create('operation #1').run);
    var process = ProcessMock.create('operation #2');
    throttler.schedule(process.run, true);

    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }

    promise
        .then(function() {
          assertThrottlerTimeout();
          timeoutMock.fireAllTimers();
          process.finish();
          return waitForProcessFinish();
        })
        .then(next);
  }

  function testAlwaysExecuteLastScheduled(next, runningProcess) {
    assertThrottlerIdle();
    var process = null;
    for (var i = 0; i < 4; ++i) {
      process = ProcessMock.create('operation #' + i);
      throttler.schedule(process.run, i % 2 === 0);
    }
    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }
    promise
        .then(function() {
          assertThrottlerTimeout();
          timeoutMock.fireAllTimers();
          process.finish();
          return waitForProcessFinish();
        })
        .then(next);
  }

  TestRunner.runTestSuite([
    testSimpleSchedule,

    testAsSoonAsPossibleOverrideTimeout,

    testAlwaysExecuteLastScheduled,

    function testSimpleScheduleDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testSimpleSchedule(next, runningProcess);
      });
    },

    function testAsSoonAsPossibleOverrideDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testAsSoonAsPossibleOverrideTimeout(next, runningProcess);
      });
    },

    function testAlwaysExecuteLastScheduledDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testAlwaysExecuteLastScheduled(next, runningProcess);
      });
    },

    function testScheduleFromProcess(next) {
      var nextProcess;
      assertThrottlerIdle();
      var process = ProcessMock.create('operation #1', processBody);
      throttler.schedule(process.run);
      assertThrottlerTimeout();
      timeoutMock.fireAllTimers();
      process.finish();
      waitForProcessFinish()
          .then(function() {
            assertThrottlerTimeout();
            timeoutMock.fireAllTimers();
            nextProcess.finish();
            return waitForProcessFinish();
          })
          .then(next);

      function processBody() {
        nextProcess = ProcessMock.create('operation #2');
        throttler.schedule(nextProcess.run, false);
      }
    },

    function testExceptionFromProcess(next) {
      var process = ProcessMock.create('operation #1', processBody);
      throttler.schedule(process.run);
      timeoutMock.fireAllTimers();
      waitForProcessFinish().then(function() {
        assertThrottlerIdle();
        next();
      });

      function processBody() {
        throw new Error('Exception during process execution.');
      }
    }
  ]);

  function waitForProcessFinish() {
    var promiseResolve;
    var hasFinished;
    TestRunner.addSniffer(Common.Throttler.prototype, '_processCompletedForTests', onFinished);
    function onFinished() {
      hasFinished = true;
      if (promiseResolve)
        promiseResolve();
    }
    return new Promise(function(success) {
      promiseResolve = success;
      if (hasFinished)
        success();
    });
  }

  function throttlerToRunningState() {
    assertThrottlerIdle();
    var process = ProcessMock.create('long operation');
    throttler.schedule(process.run);
    assertThrottlerTimeout();
    timeoutMock.fireAllTimers();
    return process;
  }

  function assertThrottlerIdle() {
    var timeouts = timeoutMock.activeTimersTimeouts();
    if (timeouts.length !== 0) {
      TestRunner.addResult(
          'ERROR: throttler is not in idle state. Scheduled timers timeouts: [' + timeouts.sort().join(', ') + ']');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult('Throttler is in IDLE state (doesn\'t have any timers set up)');
  }

  function assertThrottlerTimeout() {
    var timeouts = timeoutMock.activeTimersTimeouts();
    if (timeouts.length === 0) {
      TestRunner.addResult('ERROR: throttler is not in timeout state. Scheduled timers timeouts are empty!');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(
        'Throttler is in TIMEOUT state. Scheduled timers timeouts: [' + timeouts.sort().join(', ') + ']');
  }

  function logSchedule(operation, asSoonAsPossible) {
    TestRunner.addResult('SCHEDULED: \'' + operation.processName + '\' asSoonAsPossible: ' + asSoonAsPossible);
  }
})();
