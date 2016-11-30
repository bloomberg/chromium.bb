// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileOverview  WebAudio layout test utility library. Built around W3C's
 *                testharness.js. Includes asynchronous test task manager,
 *                assertion utilities.
 * @dependency    testharness.js
 * @seealso       webaudio/unit-tests/audit.html for actual examples.
 */


(function () {

  'use strict';

  // Selected properties from testharness.js
  let testharnessProperties = [
    'test', 'async_test', 'promise_test', 'promise_rejects',
    'generate_tests', 'setup', 'done', 'assert_true', 'assert_false'
  ];

  // Check if testharness.js is properly loaded. Throw otherwise.
  for (let name in testharnessProperties) {
    if (!self.hasOwnProperty(testharnessProperties[name])) {
      throw new Error('Cannot proceed. testharness.js is not loaded.');
      break;
    }
  }
})();


/**
 * @class Audit
 * @description A WebAudio layout test task manager.
 * @example
 *   let audit = Audit.createTaskRunner();
 *   audit.define('first-task', function (task, should) {
 *     task.describe('the first task');
 *     should(someValue).beEqualTo(someValue);
 *     task.done();
 *   });
 *   audit.run();
 */
window.Audit = (function () {

  'use strict';

  function _logPassed (message) {
    test(function (arg) {
      assert_true(true);
    }, message);
  }

  function _logFailed (message, detail) {
    test(function () {
      assert_true(false, detail);
    }, message);
  }

  function _throwException (message) {
    throw new Error(message);
  }

  // Generate a descriptive string from a target value in various types.
  function _generateDescription (target, options) {
    let targetString;

    switch (typeof target) {
      case 'object':
        // Handle Arrays.
        if (target instanceof Array || target instanceof Float32Array ||
            target instanceof Float64Array) {
          let arrayElements = target.length < options.numberOfArrayElements
              ? String(target)
              : String(target.slice(0, options.numberOfArrayElements)) + '...';
          targetString = '[' + arrayElements + ']';
        } else {
          targetString = '' + String(targetString).split(/[\s\]]/)[1];
        }
        break;
      default:
        targetString = String(target);
        break;
    }

    return targetString;
  }


  /**
   * @class Should
   * @description Assertion subtask for the Audit task.
   * @param {Task} parentTask           Associated Task object.
   * @param {Any} actual                Target value to be tested.
   * @param {String} actualDescription  String description of the test target.
   */
  class Should {

    constructor (parentTask, actual, actualDescription) {
      this._task = parentTask;

      this._actual = actual;
      this._actualDescription = (actualDescription || null);
      this._expected = null;
      this._expectedDescription = null;

      this._result = true;
      this._detail = '';

      /**
       * @param {Number} numberOfErrors   Number of errors to be printed.
       * @param {Number} numberOfArrayElements  Number of array elements to be
       *                                        printed in the test log.
       * @param {Boolean} verbose         Verbose output from the assertion.
       */
      this._options = {
        numberOfErrors: 4,
        numberOfArrayElements: 16,
        verbose: false
      };
    }

    _processArguments (args) {
      if (args.length === 0)
        return;

      if (args.length > 0)
        this._expected = args[0];

      if (typeof args[1] === 'string') {
        // case 1: (expected, description, options)
        this._expectedDescription = args[1];
        Object.assign(this._options, args[2]);
      } else if (typeof args[1] === 'object') {
        // case 2: (expected, options)
        Object.assign(this._options, args[1]);
      }
    }

    _buildResultText () {
      if (!this._actualDescription) {
        this._actualDescription =
            _generateDescription(this._actual, this._options);
      }

      if (!this._expectedDescription) {
        this._expectedDescription =
            _generateDescription(this._expected, this._options);
      }

      // For the assertion with a single operand.
      this._detail = this._detail.replace('${actual}', this._actualDescription);

      // If there is a second operand (i.e. expected value), we have to build
      // the string for it as well.
      if (this._expected) {
        this._detail = this._detail.replace(
            '${expected}', this._expectedDescription);
      }

      // If there is any property in |_options|, replace the property name
      // with the value.
      for (let name in this._options) {
        this._detail = this._detail.replace(
            '${' + name + '}',
            _generateDescription(this._options[name]));
      }
    }

    _finalize () {
      if (this._result) {
        _logPassed('  ' + this._detail);
      } else {
        _logFailed('X ' + this._detail);
      }

      // This assertion is finished, so update the parent task accordingly.
      this._task.update(this);

      // TODO(hongchan): configurable 'detail' message.
    }

    _assert (condition, passDetail, failDetail) {
      this._result = Boolean(condition);
      this._detail = this._result ? passDetail : failDetail;
      this._buildResultText();
      return this._finalize();
    }

    get result () {
      return this._result;
    }

    get detail () {
      return this._detail;
    }

    /**
     * should() assertions.
     *
     * @example All the assertions can have 1, 2 or 3 arguments:
     *   should().doAssert(expected);
     *   should().doAssert(expected, options);
     *   should().doAssert(expected, expectedDescription, options);
     *
     * @param {Any} expected                  Expected value of the assertion.
     * @param {String} expectedDescription    Description of expected value.
     * @param {Object} options                Options for assertion.
     * @param {Number} options.numberOfErrors Number of errors to be printed.
     *                                        (if applicable)
     * @param {Number} options.numberOfArrayElements  Number of array elements
     *                                                to be printed. (if
     *                                                applicable)
     * @notes Some assertions can have additional options for their specific
     *        testing.
     */

    /**
     * Check if |actual| exists.
     *
     * @example
     *   should({}, 'An empty object').exist();
     * @result
     *   "PASS   An empty object does exist."
     */
    exist () {
      return this._assert(
          this._actual !== null && this._actual !== undefined,
          '${actual} does exist.',
          '${actual} does *NOT* exist.');
    }

    /**
     * Check if |actual| operation wrapped in a function throws an exception
     * with a expected error type correctly. |expected| is optional.
     *
     * @example
     *   should(() => { let a = b; }, 'A bad code').throw();
     *   should(() => { let c = d; }, 'Assigning d to c.')
     *       .throw('ReferenceError');
     *
     * @result
     *   "PASS   A bad code threw an exception of ReferenceError."
     *   "PASS   Assigning d to c threw ReferenceError."
     */
    throw () {
      this._processArguments(arguments);

      let didThrowCorrectly = false;
      let passDetail, failDetail;

      try {
        // This should throw.
        this._actual();
        // Catch did not happen, so the test is failed.
        failDetail = '${actual} did *NOT* throw an exception.';
      } catch (error) {
        if (this._expected === undefined) {
          didThrowCorrectly = true;
          passDetail = '${actual} threw an exception of ' + error.name + '.';
        } else if (error.name === this._expected) {
          didThrowCorrectly = true;
          passDetail = '${actual} threw ${expected} : "' + error.message + '".';
        } else {
          didThrowCorrectly = false;
          failDetail = '${actual} threw "' + error.name
            + '" instead of ${expected}.';
        }
      }

      return this._assert(didThrowCorrectly, passDetail, failDetail);
    }

    /**
     * Check if |actual| operation wrapped in a function does not throws an
     * exception correctly.
     *
     * @example
     *   should(() => { let foo = 'bar'; }, 'let foo = "bar"').notThrow();
     *
     * @result
     *   "PASS   let foo = "bar" did not throw an exception."
     */
    notThrow () {
      let didThrowCorrectly = false;
      let passDetail, failDetail;

      try {
        this._actual();
        passDetail = '${actual} did not throw an exception.';
      } catch (error) {
        didThrowCorrectly = true;
        failDetail = '${actual} threw ' + error.name + ': '
            + error.message + '.';
      }

      return this._assert(!didThrowCorrectly, passDetail, failDetail);
    }

    /**
     * Check if |actual| promise is resolved correctly.
     *
     * @example
     *   should('My promise', promise).beResolve().then(nextStuff);
     *
     * @result
     *   "PASS   My promise resolved correctly."
     *   "FAIL X My promise rejected *INCORRECTLY* with _ERROR_."
     */
    beResolved () {
      return this._actual.then(function () {
          this._assert(true, '${actual} resolved correctly.', null);
        }.bind(this), function (error) {
          this._assert(false, null,
              '${actual} rejected *INCORRECTLY* with ' + error + '.');
        }.bind(this));
    }

    /**
     * Check if |actual| promise is rejected correctly.
     *
     * @example
     *   should('My promise', promise).beRejected().then(nextStuff);
     *
     * @result
     *   "PASS   My promise rejected correctly (with _ERROR_)."
     *   "FAIL X My promise resolved *INCORRECTLY*."
     */
    beRejected () {
      return this._actual.then(function () {
          this._assert(false, null, '${actual} resolved *INCORRECTLY*.');
        }.bind(this), function (error) {
          this._assert(true,
              '${actual} rejected correctly with ' + error + '.', null);
        }.bind(this));
    }

    /**
     * Check if |actual| is a boolean true.
     *
     * @example
     *   should(3 < 5, '3 < 5').beTrue();
     *
     * @result
     *   "PASS   3 < 5 is true."
     */
    beTrue () {
      return this._assert(
          this._actual === true,
          '${actual} is true.',
          '${actual} is *NOT* true.');
    }

    /**
     * Check if |actual| is a boolean false.
     *
     * @example
     *   should(3 > 5, '3 > 5').beFalse();
     *
     * @result
     *   "PASS   3 > 5 is false."
     */
    beFalse () {
      return this._assert(
          this._actual === false,
          '${actual} is false.',
          '${actual} is *NOT* false.');
    }

    /**
     * Check if |actual| is strictly equal to |expected|. (no type coercion)
     *
     * @example
     *   should(1).beEqualTo(1);
     *
     * @result
     *   "PASS   1 is equal to 1."
     */
    beEqualTo () {
      this._processArguments(arguments);
      return this._assert(
          this._actual === this._expected,
          '${actual} is equal to ${expected}.',
          '${actual} is *NOT* equal to ${expected}.');
    }

    /**
     * Check if |actual| is not equal to |expected|.
     *
     * @example
     *   should(1).notBeEqualTo(2);
     *
     * @result
     *   "PASS   1 is *NOT* equal to 2."
     */
    notBeEqualTo () {
      this._processArguments(arguments);
      return this._assert(
          this._actual !== this._expected,
          '${actual} is not equal to ${expected}.',
          '${actual} should *NOT* be equal to ${expected}.');
    }

    /**
     * Check if |actual| is greater than |expected|.
     *
     * @example
     *   should(2).beGreaterThanOrEqualTo(2);
     *
     * @result
     *   "PASS   2 is greater than or equal to 2."
     */
    beGreaterThan () {
      this._processArguments(arguments);
      return this._assert(
          this._actual > this._expected,
           '${actual} is greater than ${expected}.',
           '${actual} is *NOT* greater than ${expected}.'
        );
    }

    /**
     * Check if |actual| is greater than or equal to |expected|.
     *
     * @example
     *   should(2).beGreaterThan(1);
     *
     * @result
     *   "PASS   2 is greater than 1."
     */
    beGreaterThanOrEqualTo () {
      this._processArguments(arguments);
      return this._assert(
          this._actual >= this._expected,
           '${actual} is greater than or equal to ${expected}.',
           '${actual} is *NOT* greater than or equal to ${expected}.'
        );
    }

    /**
     * Check if |actual| is less than |expected|.
     *
     * @example
     *   should(1).beLessThan(2);
     *
     * @result
     *   "PASS   1 is less than 2."
     */
    beLessThan () {
      this._processArguments(arguments);
      return this._assert(
          this._actual < this._expected,
           '${actual} is less than ${expected}.',
           '${actual} is *NOT* less than ${expected}.'
        );
    }

    /**
     * Check if |actual| is less than or equal to |expected|.
     *
     * @example
     *   should(1).beLessThanOrEqualTo(1);
     *
     * @result
     *   "PASS   1 is less than or equal to 1."
     */
    beLessThanOrEqualTo () {
      this._processArguments(arguments);
      return this._assert(
          this._actual <= this._expected,
           '${actual} is less than or equal to ${expected}.',
           '${actual} is *NOT* less than or equal to ${expected}.'
        );
    }

    /**
     * Check if |actual| array is filled with a constant |expected| value.
     *
     * @example
     *   should([1, 1, 1]).beConstantValueOf(1);
     *
     * @result
     *   "PASS   [1,1,1] contains only the constant 1."
     */
    beConstantValueOf () {
      this._processArguments(arguments);

      let passed = true;
      let passDetail, failDetail;
      let errors = {};

      for (let index in this._actual) {
        if (this._actual[index] !== this._expected)
          errors[index] = this._actual[index];
      }

      let numberOfErrors = Object.keys(errors).length;
      passed = numberOfErrors === 0;

      if (passed) {
        passDetail = '${actual} contains only the constant ${expected}.';
      } else {
        let counter = 0;
        failDetail = '${actual} contains ' + numberOfErrors
          + ' values that are *NOT* equal to ${expected}: ';
        failDetail += '\n\tIndex\tActual';
        for (let errorIndex in errors) {
          failDetail += '\n\t[' + errorIndex + ']'
              + '\t' + errors[errorIndex];
          if (++counter >= this._options.numberOfErrors) {
            failDetail += '\n\t...and ' + (numberOfErrors - counter)
              + ' more errors.';
            break;
          }
        }
      }

      return this._assert(passed, passDetail, failDetail);
    }

    /**
     * Check if |actual| array is identical to |expected| array element-wise.
     *
     * @example
     *   should([1, 2, 3]).beEqualToArray([1, 2, 3]);
     *
     * @result
     *   "[1,2,3] is identical to the array [1,2,3]."
     */
    beEqualToArray () {
      this._processArguments(arguments);

      let passed = true;
      let passDetail, failDetail;
      let errorIndices = [];

      if (this._actual.length !== this._expected.length) {
        passed = false;
        failDetail = 'The array length does *NOT* match.';
        return this._assert(passed, passDetail, failDetail);
      }

      for (let index in this._actual) {
        if (this._actual[index] !== this._expected[index])
          errorIndices.push(index);
      }

      passed = errorIndices.length === 0;

      if (passed) {
        passDetail = '${actual} is identical to the array ${expected}.';
      } else {
        let counter = 0;
        failDetail = '${actual} contains ' + errorIndices.length
          + ' values that are *NOT* equal to ${expected}: ';
        failDetail += '\n\tIndex\tActual\t\t\tExpected';
        for (let index of errorIndices) {
          failDetail += '\n\t[' + index + ']'
            + '\t' + this._actual[index].toExponential(16)
            + '\t' + this._expected[index].toExponential(16);
          if (++counter >= this._options.numberOfErrors) {
            failDetail += '\n\t...and ' + (numberOfErrors - counter)
              + ' more errors.';
            break;
          }
        }
      }

      return this._assert(passed, passDetail, failDetail);
    }

    /**
     * Check if |actual| array contains only the values in |expected| in the
     * order of values in |expected|.
     *
     * @example
     *   Should([1, 1, 3, 3, 2], 'My random array').containValues([1, 3, 2]);
     *
     * @result
     *   "PASS   [1,1,3,3,2] contains all the expected values in the correct
     *           order: [1,3,2].
     */
    containValues () {
      this._processArguments(arguments);

      let passed = true;
      let indexActual = 0, indexExpected = 0;

      while (indexActual < this._actual.length
          && indexExpected < this._expected.length) {
        if (this._actual[indexActual] === this._expected[indexExpected]) {
          indexActual++;
        } else {
          indexExpected++;
        }
      }

      passed = !(indexActual < this._actual.length - 1
          || indexExpected < this._expected.length - 1);

      return this._assert(
          passed,
          '${actual} contains all the expected values in the correct order: '
            + '${expected}.',
          '${actual} contains an unexpected value of '
            + this._actual[indexActual] + ' at index ' + indexActual + '.');
    }

    /**
     * Check if |actual| array does not have any glitches. Note that |threshold|
     * is not optional and is to define the desired threshold value.
     *
     * @example
     *   should([0.5, 0.5, 0.55, 0.5, 0.45, 0.5]).notGlitch(0.06);
     *
     * @result
     *   "PASS   [0.5,0.5,0.55,0.5,0.45,0.5] has no glitch above the threshold
     *           of 0.06."
     *
     */
    notGlitch () {
      this._processArguments(arguments);

      let passed = true;
      let passDetail, failDetail;

      for (let index in this._actual) {
        let diff = Math.abs(this._actual[index - 1] - this._actual[index]);
        if (diff >= this._expected) {
          passed = false;
          failDetail = '${actual} has a glitch at index ' + index + ' of size '
            + diff + '.';
        }
      }

      passDetail =
        '${actual} has no glitch above the threshold of ${expected}.';

      return this._assert(passed, passDetail, failDetail);
    }

    /**
     * Check if |actual| is close to |expected| using the given relative error
     * |threshold|.
     *
     * @example
     *   should(2.3).beCloseTo(2, 0.3);
     *
     * @result
     *   "PASS    2.3 is 2 within an error of 0.3."
     *
     * @param {Number} options.threshold    Threshold value for the comparison.
     */
    beCloseTo () {
      this._processArguments(arguments);

      let absExpected = this._expected ? Math.abs(this._expected) : 1;
      let error = Math.abs(this._actual - this._expected) / absExpected;

      return this._assert(
        error < this._options.threshold,
        '${actual} is ${expected} within an error of ${threshold}',
        '${actual} is not ${expected} within a error of ${threshold}: ' +
          '${actual} with error of ${threshold}.');
    }

    /**
     * Check if |target| array is close to |expected| array element-wise within
     * a certain error bound given by the |options|.
     *
     * The error criterion is:
     *   abs(actual[k] - expected[k]) < max(absErr, relErr * abs(expected))
     *
     * If nothing is given for |options|, then absErr = relErr = 0. If
     * absErr = 0, then the error criterion is a relative error. A non-zero
     * absErr value produces a mix intended to handle the case where the
     * expected value is 0, allowing the target value to differ by absErr from
     * the expected.
     *
     * @param {Number} options.absoluteThreshold    Absolute threshold.
     * @param {Number} options.relativeThreshold    Relative threshold.
     */
    beCloseToArray () {
      this._processArguments(arguments);

      let passed = true;
      let passDetail, failDetail;

      // Parsing options.
      let absErrorThreshold = (this._options.absoluteThreshold || 0);
      let relErrorThreshold = (this._options.relativeThreshold || 0);

      // A collection of all of the values that satisfy the error criterion.
      // This holds the absolute difference between the target element and the
      // expected element.
      let errors = {};

      // Keep track of the max absolute error found.
      let maxAbsError = -Infinity, maxAbsErrorIndex = -1;

      // Keep track of the max relative error found, ignoring cases where the
      // relative error is Infinity because the expected value is 0.
      let maxRelError = -Infinity, maxRelErrorIndex = -1;

      for (let index in this._expected) {
        let diff = Math.abs(this._actual[index] - this._expected[index]);
        let absExpected = Math.abs(this._expected[index]);
        let relError = diff / absExpected;

        if (diff > Math.max(absErrorThreshold,
                            relErrorThreshold * absExpected)) {

          if (diff > maxAbsError) {
            maxAbsErrorIndex = index;
            maxAbsError = diff;
          }

          if (!isNaN(relError) && relError > maxRelError) {
            maxRelErrorIndex = index;
            maxRelError = relError;
          }

          errors[index] = diff;
        }
      }

      let numberOfErrors = Object.keys(errors).length;
      let maxAllowedErrorDetail = JSON.stringify({
        absoluteThreshold: absErrorThreshold,
        relativeThreshold: relErrorThreshold
      });

      if (numberOfErrors === 0) {
        // The assertion was successful.
        passDetail = '${actual} equals ${expected} with an element-wise '
          + 'tolerance of ' + maxAllowedErrorDetail + '.';
      } else {
        // Failed. Prepare the detailed failure log.
        passed = false;
        failDetail = '${actual} does not equal ${expected} with an '
          + 'element-wise tolerance of ' + maxAllowedErrorDetail + '.\n';

        // Print out actual, expected, absolute error, and relative error.
        let counter = 0;
        failDetail += '\tIndex\tActual\t\t\tExpected\t\tAbsError'
            + '\t\tRelError\t\tTest threshold';
        for (let index in errors) {
          failDetail += '\n\t[' + index + ']\t'
              + this._actual[index].toExponential(16) + '\t'
              + this._expected[index].toExponential(16) + '\t'
              + errors[index].toExponential(16) + '\t'
              + (errors[index] / Math.abs(this._expected[index]))
                  .toExponential(16) + '\t'
              + Math.max(absErrorThreshold,
                    relErrorThreshold * Math.abs(this._expected[index]))
                      .toExponential(16);
          if (++counter > this._options.numberOfErrors)
            break;
        }

        // Finalize the error log: print out the location of both the maxAbs
        // error and the maxRel error so we can adjust thresholds appropriately
        // in the test.
        failDetail += '\n'
            + '\tMax AbsError of ' + maxAbsError.toExponential(16)
            + ' at index of ' + maxAbsErrorIndex + '.\n'
            + '\tMax RelError of ' + maxRelError.toExponential(16)
            + ' at index of ' + maxRelErrorIndex + '.';
      }

      return this._assert(passed, passDetail, failDetail);
    }

  }


  // Task Class state enum.
  const TaskState = {
    PENDING: 0,
    STARTED: 1,
    FINISHED: 2
  };


  /**
   * @class Task
   * @description WebAudio testing task. Managed by TaskRunner.
   */
  class Task {

    constructor (taskRunner, taskLabel, taskFunction) {
      this._taskRunner = taskRunner;
      this._taskFunction = taskFunction;
      this._label = taskLabel;
      this._description = '';
      this._state = TaskState.PENDING;
      this._result = true;

      this._totalAssertions = 0;
      this._failedAssertions = 0;
    }

    // Set the description of this task. This is printed out in the test
    // result.
    describe (message) {
      this._description = message;
      _logPassed('> [' + this._label + '] '
          + this._description);
    }

    get state () {
      return this._state;
    }

    get result () {
      return this._result;
    }

    // Start the assertion chain.
    should (actual, actualDescription) {
      return new Should(this, actual, actualDescription);
    }

    // Run this task. |this| task will be passed into the user-supplied test
    // task function.
    run () {
      this._state = TaskState.STARTED;
      this._taskFunction(
          this,
          this.should.bind(this));
    }

    // Update the task success based on the individual assertion/test inside.
    update (subTask) {
      // After one of tests fails within a task, the result is irreversible.
      if (subTask.result === false) {
        this._result = false;
        this._failedAssertions++;
      }

      this._totalAssertions++;
    }

    // Finish the current task and start the next one if available.
    done () {
      this._state = TaskState.FINISHED;

      let message = '< [' + this._label + '] ';

      if (this._result) {
        message += 'All assertion passed. (total ' + this._totalAssertions
          + ' assertions)';
        _logPassed(message);
      } else {
        message += this._failedAssertions + ' out of ' + this._totalAssertions
          + ' assertions were failed.'
        _logFailed(message);
      }

      this._taskRunner._runNextTask();
    }

    isPassed () {
      return this._state === TaskState.FINISHED && this._result;
    }

    toString () {
      return '"' + this._label + '": ' + this._description;
    }

  }


  /**
   * @class TaskRunner
   * @description WebAudio testing task runner. Manages tasks.
   */
  class TaskRunner {

    constructor () {
      this._tasks = {};
      this._taskSequence = [];
      this._currentTaskIndex = -1;

      // Configure testharness.js for the async operation.
      setup (new Function (), {
        explicit_done: true
      });
    }

    _runNextTask () {
      if (this._currentTaskIndex < this._taskSequence.length) {
        this._tasks[this._taskSequence[this._currentTaskIndex++]].run();
      } else {
        this._finish();
      }
    }

    _finish () {
      let numberOfFailures = 0;
      for (let taskIndex in this._taskSequence) {
        let task = this._tasks[this._taskSequence[taskIndex]];
        numberOfFailures += task.result ? 0 : 1;
      }

      let prefix = '# AUDIT TASK RUNNER FINISHED: ';
      if (numberOfFailures > 0) {
        _logFailed(prefix + numberOfFailures + ' out of '
          + this._taskSequence.length + ' tasks were failed.');
      } else {
        _logPassed(prefix + this._taskSequence.length
          + ' tasks ran successfully.');
      }

      // From testharness.js, report back to the test infrastructure that
      // the task runner completed all the tasks.
      done();
    }

    define (taskLabel, taskFunction) {
      if (this._tasks.hasOwnProperty(taskLabel)) {
        _throwException('Audit.define:: Duplicate task definition.');
        return;
      }

      this._tasks[taskLabel] = new Task(this, taskLabel, taskFunction);
      this._taskSequence.push(taskLabel);
    }

    // Start running all the tasks scheduled. Multiple task names can be passed
    // to execute them sequentially. Zero argument will perform all defined
    // tasks in the order of definition.
    run () {
      // Display the beginning of the test suite.
      _logPassed('# AUDIT TASK RUNNER STARTED.');

      // If the argument is specified, override the default task sequence with
      // the specified one.
      if (arguments.length > 0) {
        this._taskSequence = [];
        for (let i = 0; arguments.length; i++) {
          let taskLabel = arguments[i];
          if (!this._tasks.hasOwnProperty(taskLabel)) {
            _throwException('Audit.run:: undefined task.');
          } else if (this._taskSequence.includes(taskLabel)) {
            _throwException('Audit.run:: duplicate task request.');
          } else {
            this._taskSequence.push[taskLabel];
          }
        }
      }

      if (this._taskSequence.length === 0) {
        _throwException('Audit.run:: no task to run.');
        return;
      }

      // Start the first task.
      this._currentTaskIndex = 0;
      this._runNextTask();
    }

  }


  return {

    /**
     * Creates an instance of Audit task runner.
     */
    createTaskRunner: function () {
      return new TaskRunner();
    }

  };

})();
