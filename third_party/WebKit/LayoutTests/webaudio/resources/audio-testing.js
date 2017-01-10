/* global self */

// testharness.js has the higher priority.
var TESTHARNESS = true;
var JSTEST = false;

(function () {
    // Selected properies from testharness.js
    var testharnessProperties = [
        'test', 'async_test', 'promise_test', 'promise_rejects',
        'generate_tests', 'setup', 'done', 'assert_true', 'assert_false'
    ];

    // Selected properties from js-test.js
    var jsTestProperties = [
        'isJsTest', 'testPassed', 'testFailed', 'gc', 'finishJSTest'
    ];

    // Check if testharness.js is properly loaded and set up a flag for it.
    for (var name in testharnessProperties) {
        if (!self.hasOwnProperty(testharnessProperties[name])) {
            TESTHARNESS = false;
            break;
        }
    }

    // Immediately return here because testharness.js is ready.
    if (TESTHARNESS)
        return;

    // Because testharness.js is not loaded, let us assume that js-test.js might
    // be in use. Check if js-test.js is properly loaded and set up a flag for
    // it.
    JSTEST = true;
    for (var name in jsTestProperties) {
        if (!self.hasOwnProperty(jsTestProperties[name])) {
            JSTEST = false;
            break;
        }
    }

    // If both are not loaded at all, throw here.
    if (!JSTEST)
        throw new Error('Cannot proceed. No test infrastructure is loaded.');
})();


// |Audit| is a task runner for web audio test. It makes asynchronous web audio
// testing simple and manageable.
//
// EXAMPLE:
//
//   var audit = Audit.createTaskRunner();
//   // Define test routine. Make sure to call done() when reached at the end.
//   audit.defineTask('foo', function (done) {
//     var context = new AudioContext();
//     // do things
//     context.oncomplete = function () {
//       // verification here
//       done();
//     };
//   });
//
//   audit.defineTask('bar', function (done) {
//     // your code here
//     done();
//   });
//
//   // Queue tasks by readable task names.
//   audit.runTasks('foo', 'bar');
//
//   // Alternatively, if you want to run all of the defined tasks in the order in which they were
//   // defined, use no arguments:
//   audit.runTasks();
 var Audit = (function () {

    'use strict';

    function Tasks() {
        this.tasks = {};
        this.queue = [];
        this.currentTask = 0;
    }

    // This is to prime the task runner for the testharness.js async operation.
    Tasks.prototype._initialize = function () {
        if (TESTHARNESS) {
            setup(new Function(), {
                explicit_done: true
            });
        }
    };

    // Finalize the task runner by notifying testharness and testRunner that
    // all the task is completed.
    Tasks.prototype._finalize = function () {
        if (TESTHARNESS) {
            // From testharness.js
            done();
        }
    };

    Tasks.prototype.defineTask = function (taskName, taskFunc) {
        // Check if there is a task defined with the same name.  If found, do
        // not add the task to the roster.
        if (this.tasks.hasOwnProperty(taskName)) {
            debug('>> Audit.defineTask:: Duplicate task definition. ("' + taskName + '")');
            return;
        }

        this.tasks[taskName] = taskFunc;

        // Push the task name in the order of definition.
        this.queue.push(taskName);
    };

    // If arguments are given, only run the requested tasks.  Check for any
    // undefined or duplicate task in the requested task arguments.  If there
    // is no argument, run all the defined tasks.
    Tasks.prototype.runTasks = function () {

        this._initialize();

        if (arguments.length > 0) {

            // Reset task queue and refill it with the with the given arguments,
            // in argument order.
            this.queue = [];

            for (var i = 0; i < arguments.length; i++) {
                if (!this.tasks.hasOwnProperty(arguments[i]))
                    debug('>> Audit.runTasks:: Ignoring undefined task. ("' + arguments[i] + '")');
                else if (this.queue.indexOf(arguments[i]) > -1)
                    debug('>> Audit.runTasks:: Ignoring duplicate task request. ("' + arguments[i] + '")');
                else
                    this.queue.push(arguments[i]);
            }
        }

        if (this.queue.length === 0) {
            debug('>> Audit.runTasks:: No task to run.');
            return;
        }

        // taskDone() callback from each task.  Increase the task index and call
        // the next task.  Note that explicit signaling by taskDone() in each
        // task is needed because some of tests run asynchronously.
        var taskDone = function () {
            if (this.currentTask !== this.queue.length - 1) {
                ++this.currentTask;
                // debug('>> Audit.runTasks: ' + this.queue[this.currentTask]);
                this.tasks[this.queue[this.currentTask]](taskDone);
            } else {
                this._finalize();
            }
            return;
        }.bind(this);

        // Start the first task.
        // debug('>> Audit.runTasks: ' + this.queue[this.currentTask]);
        this.tasks[this.queue[this.currentTask]](taskDone);
    };

    return {
        createTaskRunner: function () {
            return new Tasks();
        }
    };

})();


// |Should| JS layout test utility.
// Dependency: ../resources/js-test.js
var Should = (function () {

    'use strict';

    // ShouldModel internal class. For the exposed (factory) method, it is the
    // return value of this closure.
    function ShouldModel(desc, target, opts) {
        this.desc = desc;
        this.target = target;

        // Check if the target contains any NaN value.
        this._checkNaN(this.target, 'ACTUAL');

        // |_testPassed| and |_testFailed| set this appropriately.
        this._success = false;

        // If the number of errors is greater than this, the rest of error
        // messages are suppressed. the value is fairly arbitrary, but shouldn't
        // be too small or too large.
        this.NUM_ERRORS_LOG = opts.numberOfErrorLog;

        // If the number of array elements is greater than this, the rest of
        // elements will be omitted.
        this.NUM_ARRAY_LOG = opts.numberOfArrayLog;

        // If true, verbose output for the failure case is printed, for methods where this makes
        // sense.
        this.verbose = !opts.brief;

        // If set, this is the precision with which numbers will be printed.
        this.PRINT_PRECISION = opts.precision;
    }

    // Internal methods starting with a underscore.
    ShouldModel.prototype._testPassed = function (msg, addNewline) {
        this._success = true;
        var newLine = addNewline ? '\n' : '';
        if (TESTHARNESS) {
            // Using testharness.js
            test(function () {
                assert_true(true);
                }, this.desc + ' ' + msg + '.' + newLine);
        } else {
            // Using js-test.js
            testPassed(this.desc + ' ' + msg + '.' + newLine);
        }
    };

    ShouldModel.prototype._testFailed = function (msg, addNewline) {
        this._success = false;
        var that = this;
        var newLine = addNewline ? '\n' : '';
        if (TESTHARNESS) {
            test(function () {
                assert_true(false, that.desc + ' ' + msg + '.' + newLine);
                }, this.desc);
        } else {
            testFailed(this.desc + ' ' + msg + '.' + newLine);
        }
    };

    ShouldModel.prototype._isArray = function (arg) {
      return arg instanceof Array || arg instanceof Float32Array || arg instanceof Uint8Array ||
        arg instanceof Uint16Array || arg instanceof Uint32Array || arg instanceof Int8Array ||
        arg instanceof Int16Array || arg instanceof Int32Array || arg instanceof Uint8ClampedArray ||
        arg instanceof Float64Array;
    };

    ShouldModel.prototype._assert = function (expression, reason, value) {
        if (expression)
            return;

        var failureMessage = 'Assertion failed: ' + reason + ' ' + this.desc +'.';
        if (arguments.length >= 3)
            failureMessage += ": " + value;

        if (TESTHARNESS) {
            test(function () {
                assert_true(false, reason + ' (' + value + ')');
            }, this.desc)
        } else {
            testFailed(failureMessage);
        }

        throw failureMessage;
    };

    // Check the expected value if it is a NaN (Number) or has NaN(s) in
    // its content (Array or Float32Array). Returns a string depends on the
    // result of check.
    ShouldModel.prototype._checkNaN = function (value, label) {
        var failureMessage = 'NaN found in ' + label + ' while testing "' +
            this.desc + '"';

        // Checking a single variable first.
        if (Number.isNaN(value)) {
            if (TESTHARNESS) {
                test(function () {
                    assert_true(false, failureMessage);
                }, this.desc)
            } else {
                testFailed(failureMessage);
            }

            throw failureMessage;
        }

        // If the value is not a NaN nor array, we can assume it is safe.
        if (!this._isArray(value))
            return;

        // Otherwise, check the array array.
        var indices = [];
        for (var i = 0; i < value.length; i++) {
            if (Number.isNaN(value[i]))
                indices.push(i);
        }

        if (indices.length === 0)
            return;

        var failureDetail = ' (' + indices.length + ' instances total)\n';
        for (var n = 0; n < indices.length; n++) {
            failureDetail += '   >> [' + indices[n] + '] = NaN\n';
            if (n >= this.NUM_ERRORS_LOG) {
                failureDetail += ' and ' + (indices.length - n) +
                ' more NaNs...';
                break;
            }
        }

        if (TESTHARNESS) {
            test(function () {
                assert_true(false, failureMessage + failureDetail);
            }, this.desc)
        } else {
            testFailed(failureMessage + failureDetail);
        }

        throw failureMessage;
    };

    // Check if |target| exists.
    //
    // Example:
    // Should('Object', {}).exist();
    // Result:
    // "PASS Object exists."
    ShouldModel.prototype.exist = function () {
        if (this.target !== null && this.target !== undefined) {
            this._testPassed('exists');
        } else {
            this._testFailed('does not exist');
        }

        return this._success;
    };

    // Check if |target| is equal to |value|.
    //
    // Example:
    // Should('Zero', 0).beEqualTo(0);
    // Result:
    // "PASS Zero is equal to 0."
    ShouldModel.prototype.beEqualTo = function (value) {
        if (value != null) {
            var type = typeof value;
            this._assert(type === 'number' || type === 'string' || type === 'boolean',
                         'value should be number, string, or boolean for', value);
        }

        this._checkNaN(value, 'EXPECTED');

        var outputValue = value;
        if (type === 'string')
            outputValue = '"' + outputValue + '"';
        if (this.target === value) {
            var outputValue = (type === 'string') ? '"' + value + '"' : value;
            this._testPassed('is equal to ' + outputValue);
        } else {
            var targetValue = this.target;
            if (typeof this.target === 'string')
                targetValue = '"' + targetValue + '"';
            this._testFailed('was ' + targetValue + ' instead of ' + outputValue);
        }
        return this._success;
    };

    // Check if |target| is not equal to |value|.
    //
    // Example:
    // Should('One', one).notBeEqualTo(0);
    // Result:
    // "PASS One is not equal to 0."
    ShouldModel.prototype.notBeEqualTo = function (value) {
        var type = typeof value;
        this._assert(type === 'number' || type === 'string' || type === "boolean",
            'value should be number, string, or boolean for', value);

        this._checkNaN(value, 'EXPECTED');

        if (this.target === value)
            this._testFailed('should not be equal to ' + value);
        else
            this._testPassed('is not equal to ' + value);
        return this._success;
    };

    // Check if |target| is greater than or equal to |value|.
    //
    // Example:
    // Should("SNR", snr).beGreaterThanOrEqualTo(100);
    // Result:
    // "PASS SNR exceeds 100"
    // "FAIL SNR (n) is not greater than or equal to 100"
    ShouldModel.prototype.beGreaterThanOrEqualTo = function (value) {
        var type = typeof value;
        this._assert(type === 'number' || type === 'string',
            'value should be number or string for', value);

        this._checkNaN(value, 'EXPECTED');

        var prefix = '(' + this.target + ') ';

        if (this.target >= value) {
            if (!this.verbose)
                prefix = '';
            this._testPassed(prefix + "is greater than or equal to " + value);
        } else {
            this._testFailed(prefix + "is not greater than or equal to " + value);
        }
        return this._success;
    }

    // Check if |target| is greater than |value|.
    //
    // Example:
    // Should("SNR", snr).beGreaterThan(100);
    // Result:
    // "PASS SNR is greater than 100"
    // "FAIL SNR (n) is not greater than 100"
    ShouldModel.prototype.beGreaterThan = function (value) {
        var type = typeof value;
        this._assert(type === 'number' || type === 'string',
            'value should be number or string for', value);

        this._checkNaN(value, 'EXPECTED');

        if (this.target > value)
            this._testPassed("is greater than " + value);
        else
            this._testFailed("(" + this.target + ") is not greater than " + value);
        return this._success;
    }

    // Check if |target| is lest than or equal to |value|.
    //
    // Example:
    // maxError = 1e-6;
    // Should("max error", maxError).beLessThanOrEqualTo(1e-5);
    // Should("max error", maxError).beLessThanOrEqualTo(-1);
    // Result:
    // "PASS max error is less than or equal to 1e-5"
    // "FAIL max error (1e-6) is not less than or equal to -1"
    ShouldModel.prototype.beLessThanOrEqualTo = function (value) {
        var type = typeof value;
        this._assert(type === 'number', 'value should be number or string for', value);

        this._checkNaN(value, 'EXPECTED');

        var prefix = '(' + this.target + ') ';

        if (this.target <= value) {
            if (!this.verbose)
                prefix = '';
            this._testPassed(prefix + "is less than or equal to " + value);
        } else {
            this._testFailed(prefix + "is not less than or equal to " + value);
        }
        return this._success;
    }

    // Check if |target| is close to |value| using the given relative error |threshold|.  |value|
    // should not be zero, but no check is made for that.  The |target| value is printed to
    // precision |precision|, with |precision| defaulting to 7.
    //
    // If |value| is 0, however, |threshold| is treated as an absolute threshold.
    //
    // Example:
    // Should("One", 1.001).beCloseTo(1, .1);
    // Should("One", 2).beCloseTo(1, .1);
    // Result:
    // "PASS One is 1 within a relative error of 0.1."
    // "FAIL One is not 1 within a relative error of 0.1: 2"
    ShouldModel.prototype.beCloseTo = function (value, errorThreshold) {
        var type = typeof value;
        this._assert(type === 'number', 'value should be number for', value);

        this._checkNaN(value, 'EXPECTED');

        if (value) {
            var relativeError = Math.abs(this.target - value) / Math.abs(value);
            if (relativeError <= errorThreshold) {
                this._testPassed("is " + value.toPrecision(this.PRINT_PRECISION) +
                    " within a relative error of " + errorThreshold);
            } else {
                // Include actual relative error so the failed test case can be updated with the actual
                // relative error, if appropriate.
                this._testFailed("is not " + value.toPrecision(this.PRINT_PRECISION) +
                    " within a relative error of " + errorThreshold +
                    ": " + this.target + " with relative error " + relativeError
                );
            }
        } else {
            var absoluteError = Math.abs(this.target - value);
            if (absoluteError <= errorThreshold) {
                this._testPassed("is " + value.toPrecision(this.PRINT_PRECISION) +
                    " within an absolute error of " + errorThreshold);
            } else {
                // Include actual absolute error so the failed test case can be updated with the
                // actual error, if appropriate.
                this._testFailed("is not " + value.toPrecision(this.PRINT_PRECISION) +
                    " within an absolute error of " + errorThreshold +
                    ": " + this.target + " with absolute error " + absoluteError
                );
            }
        }
        return this._success;
    }

    // Check if |func| throws an exception with a certain |errorType| correctly.
    // |errorType| is optional.
    //
    // Example:
    // Should('A bad code', function () { var a = b; }).throw();
    // Result:
    // "PASS A bad code threw an exception."
    // Example:
    // Should('var c = d', function () { var c = d; }).throw('ReferenceError');
    // "PASS var c = d threw ReferenceError."
    ShouldModel.prototype.throw = function (errorType) {
        if (typeof this.target !== 'function') {
            console.log('target is not a function. test halted.');
            return;
        }

        try {
            this.target();
            this._testFailed('did not throw an exception');
        } catch (error) {
            if (errorType === undefined)
                this._testPassed('threw an exception of type ' + error.name);
            else if (error.name === errorType)
                this._testPassed('threw ' + errorType + ': ' + error.message);
            else if (self.hasOwnProperty(errorType) && error instanceof self[errorType])
                this._testPassed('threw ' + errorType + ': ' + error.message);
            else
                this._testFailed('threw ' + error.name + ' instead of ' + errorType);
        }
        return this._success;
    };

    // Check if |func| does not throw an exception.
    //
    // Example:
    // Should('var foo = "bar"', function () { var foo = 'bar'; }).notThrow();
    // Result:
    // "PASS var foo = "bar" did not throw an exception."
    ShouldModel.prototype.notThrow = function () {
        try {
            this.target();
            this._testPassed('did not throw an exception');
        } catch (error) {
            this._testFailed('threw ' + error.name + ': ' + error.message);
        }
        return this._success;
    };

    // Check if |target| array is filled with constant values.
    //
    // Example:
    // Should('[2, 2, 2]', [2, 2, 2]).beConstantValueOf(2);
    // Result:
    // "PASS [2, 2, 2] has constant values of 2."
    ShouldModel.prototype.beConstantValueOf = function (value) {
        this._checkNaN(value, 'EXPECTED');

        var mismatches = {};
        for (var i = 0; i < this.target.length; i++) {
            if (this.target[i] !== value)
            mismatches[i] = this.target[i];
        }

        var numberOfmismatches = Object.keys(mismatches).length;

        if (numberOfmismatches === 0) {
            this._testPassed('contains only the constant ' + value);
        } else {
            var counter = 0;
            var failureMessage = 'contains ' + numberOfmismatches +
            ' values that are NOT equal to ' + value + ':';
            for (var index in mismatches) {
                failureMessage += '\n[' + index + '] : ' + mismatches[index];
                if (++counter >= this.NUM_ERRORS_LOG) {
                    failureMessage += '\nand ' + (numberOfmismatches - counter) +
                    ' more differences...';
                    break;
                }
            }
            this._testFailed(failureMessage);
        }
        return this._success;
    };

    // Check if |target| array is identical to |expected| array element-wise.
    //
    // Example:
    // Should('[1, 2, 3]', [1, 2, 3]).beEqualToArray([1, 2, 3]);
    // Result:
    // "PASS [1, 2, 3] is identical to the array [1,2,3]."
    ShouldModel.prototype.beEqualToArray = function (array) {
        this._assert(this._isArray(array) && this.target.length === array.length,
            'Invalid array or the length does not match.', array);

        this._checkNaN(array, 'EXPECTED');

        var mismatches = {};
        for (var i = 0; i < this.target.length; i++) {
            if (this.target[i] !== array[i])
                mismatches[i] = this.target[i];
        }

        var numberOfmismatches = Object.keys(mismatches).length;
        var arrSlice = array.slice(0, this.NUM_ARRAY_LOG);
        var arrStr = arrSlice[0].toPrecision(this.PRINT_PRECISION);
        for (var k = 1; k < arrSlice.length; ++k)
            arrStr += ',' + arrSlice[k].toPrecision(this.PRINT_PRECISION);
        if (array.length > this.NUM_ARRAY_LOG)
            arrStr += ',...';

        if (numberOfmismatches === 0) {
            this._testPassed('is identical to the array [' + arrStr + ']');
        } else {
            var counter = 0;
            var failureMessage = 'is not equal to the array [' + arrStr + ']';
            if (this.verbose)
                failureMessage += '\nindex\tActual\t\tExpected';
            for (var index in mismatches) {
                failureMessage += '\n[' + index + '] : ' + mismatches[index];
                if (this.verbose)
                    failureMessage += '\t' + array[index];
                if (++counter >= this.NUM_ERRORS_LOG) {
                    failureMessage += '\nand ' + (numberOfmismatches - counter) +
                    ' more differences...';
                    break;
                }
            }

            this._testFailed(failureMessage);
        }
        return this._success;
    };

    // Check if |target| array is close to |expected| array element-wise within
    // an certain error bound given by |absoluteThresholdOrOptions|.
    //
    // The error criterion is:
    //
    //   Math.abs(target[k] - expected[k]) < Math.max(abserr, relerr * Math.abs(expected))
    //
    // If |absoluteThresholdOrOptions| is a number, t, then abserr = t and relerr = 0.  That is the
    // max difference is bounded by t.
    //
    // If |absoluteThresholdOrOptions| is a property bag, then abserr is the value of the
    // absoluteThreshold property and relerr is the value of the relativeThreshold property.  If
    // nothing is given, then abserr = relerr = 0.  If abserr = 0, then the error criterion is a
    // relative error.  A non-zero abserr value produces a mix intended to handle the case where the
    // expected value is 0, allowing the target value to differ by abserr from the expected.
    //
    // Example:
    // Should('My array', [0.11, 0.19]).beCloseToArray([0.1, 0.2], 0.02);
    // Result:
    // "PASS My array equals [0.1,0.2] within an element-wise tolerance of 0.02."
    ShouldModel.prototype.beCloseToArray = function (expected, absoluteThresholdOrOptions) {
        // For the comparison, the target length must be bigger than the expected.
        this._assert(this.target.length >= expected.length,
            'The target array length must be longer than ' + expected.length +
            ' but got ' + this.target.length + '.');

        this._checkNaN(expected, 'EXPECTED');

        var absoluteErrorThreshold = 0;
        var relativeErrorThreshold = 0;

        // A collection of all of the values that satisfy the error criterion.  This holds the
        // absolute difference between the target element and the expected element.
        var mismatches = {};

        // Keep track of the max absolute error found
        var maxAbsError = -Infinity;
        var maxAbsErrorIndex = -1;
        // Keep trac of the max relative error found, ignoring cases where the relative error is
        // Infinity because the expected value is 0.
        var maxRelError = -Infinity;
        var maxRelErrorIndex = -1;

        // A number or string for printing out the actual thresholds used for the error criterion.
        var maxAllowedError;

        // Set up the thresholds based on |absoluteThresholdOrOptions|.
        if (typeof(absoluteThresholdOrOptions) === 'number') {
            absoluteErrorThreshold = absoluteThresholdOrOptions;
            maxAllowedError = absoluteErrorThreshold;
        } else {
            var opts = absoluteThresholdOrOptions;
            if (opts.hasOwnProperty('absoluteThreshold'))
                absoluteErrorThreshold = opts.absoluteThreshold;
            if (opts.hasOwnProperty('relativeThreshold'))
                relativeErrorThreshold = opts.relativeThreshold;
            maxAllowedError = '{absoluteThreshold: ' + absoluteErrorThreshold
              + ', relativeThreshold: ' + relativeErrorThreshold
              + '}';
        }

        for (var i = 0; i < expected.length; i++) {
            var diff = Math.abs(this.target[i] - expected[i]);
            if (diff > Math.max(absoluteErrorThreshold, relativeErrorThreshold * Math.abs(expected[i]))) {
                mismatches[i] = diff;
                // Keep track of the location of the absolute max difference.
                if (diff > maxAbsError) {
                    maxAbsErrorIndex = i;
                    maxAbsError = diff;
                }
                // Keep track of the location of the max relative error, ignoring cases where the
                // relative error is NaN.
                var relError = diff / Math.abs(expected[i]);
                if (!isNaN(relError) && relError > maxRelError) {
                    maxRelErrorIndex = i;
                    maxRelError = relError;
                }
            }
        }

        var numberOfmismatches = Object.keys(mismatches).length;
        var arrSlice = expected.slice(0, Math.min(expected.length, this.NUM_ARRAY_LOG));
        var arrStr;

        arrStr = arrSlice[0].toPrecision(this.PRINT_PRECISION);
        for (var k = 1; k < arrSlice.length; ++k)
            arrStr += ',' + arrSlice[k].toPrecision(this.PRINT_PRECISION);

        if (expected.length > this.NUM_ARRAY_LOG)
            arrStr += ',...';
        if (numberOfmismatches === 0) {
            this._testPassed('equals [' + arrStr +
                '] with an element-wise tolerance of ' + maxAllowedError);
        } else {
            var counter = 0;
            var failureMessage = 'does not equal [' + arrStr +
                '] with an element-wise tolerance of ' + maxAllowedError;

            // Print a nice header for the  table to follow.
            if (this.verbose)
                failureMessage += "\nIndex     Actual                  Expected                Diff                   Relative";
            else
                failureMessage += "\nDifference between expected and actual:";

            for (var index in mismatches) {
                failureMessage += '\n[' + index + ']:    ';
                if (this.verbose) {
                    // When verbose, print out actual, expected, absolute error, and relative error.
                    // TODO: print these out in nice columns to make it easier to read.
                    var relError = Math.abs(this.target[index] - expected[index]) / Math.abs(expected[index]);
                    failureMessage += this.target[index].toExponential(16) + '   '
                            + expected[index].toExponential(16) + '   '
                            + mismatches[index].toExponential(16) + '  '
                            + relError.toExponential(16) + '  '
                            + Math.max(absoluteErrorThreshold,
                                       relativeErrorThreshold * Math.abs(expected[index]));
                } else {
                    // Otherwise, just the print the absolute error.
                    failureMessage += mismatches[index];
                }
                if (++counter >= this.NUM_ERRORS_LOG) {
                    failureMessage += '\nand ' + (numberOfmismatches - counter) +
                            ' more differences, with max absolute error';
                    if (this.verbose) {
                        // When verbose, print out the location of both the max absolute error and
                        // the max relative error so we can adjust thresholds appropriately in the
                        // test.
                        var relError = Math.abs(this.target[maxAbsErrorIndex] - expected[maxAbsErrorIndex])
                                / Math.abs(expected[maxAbsErrorIndex]);
                        failureMessage += ' at index ' + maxAbsErrorIndex + ':';
                        failureMessage += '\n[' + maxAbsErrorIndex + ']:    ';
                        failureMessage += this.target[maxAbsErrorIndex].toExponential(16) + '   '
                                + expected[maxAbsErrorIndex].toExponential(16) + '   '
                                + mismatches[maxAbsErrorIndex].toExponential(16) + '   '
                                + relError.toExponential(16) + '  '
                                + Math.max(absoluteErrorThreshold,
                                    relativeErrorThreshold * Math.abs(expected[maxAbsErrorIndex]));
                        failureMessage += '\nand max relative error';
                        failureMessage += ' at index ' + maxRelErrorIndex + ':';
                        failureMessage += '\n[' + maxRelErrorIndex + ']:    ';
                        failureMessage += this.target[maxRelErrorIndex].toExponential(16) + '   '
                                + expected[maxRelErrorIndex].toExponential(16) + '   '
                                + mismatches[maxRelErrorIndex].toExponential(16) + '   '
                                + maxRelError.toExponential(16) + '  '
                                + Math.max(absoluteErrorThreshold,
                                    relativeErrorThreshold * Math.abs(expected[maxRelErrorIndex]));
                    } else {
                        // Not verbose, so just print out the max absolute error
                        failureMessage += ' of ' + maxAbsError + ' at index ' + maxAbsErrorIndex;
                    }
                    break;
                }
            }

            this._testFailed(failureMessage);
        }
        return this._success;
    };

    // Check if |target| array contains a set of values in a certain order.
    //
    // Example:
    // Should('My random array', [1, 1, 3, 3, 2]).containValues([1, 3, 2]);
    // Result:
    // "PASS My random array contains all the expected values in the correct
    //  order: [1,3,2]."
    ShouldModel.prototype.containValues = function (expected) {
        this._checkNaN(expected, 'EXPECTED');

        var indexExpected = 0, indexActual = 0;
        while (indexExpected < expected.length && indexActual < this.target.length) {
            if (expected[indexExpected] === this.target[indexActual])
                indexActual++;
            else
                indexExpected++;
        }

        if (indexExpected < expected.length-1 || indexActual < this.target.length-1) {
            this._testFailed('contains an unexpected value ' + this.target[indexActual] +
            ' at index ' + indexActual);
        } else {
            this._testPassed('contains all the expected values in the correct order: [' +
            expected + ']');
        }
        return this._success;
    };

    // Check if |target| array does not have any glitches. Note that |threshold|
    // is not optional and is to define the desired threshold value.
    //
    // Example:
    // Should('Channel #0', chanL).notGlitch(0.0005);
    // Result:
    // "PASS Channel #0 has no glitch above the threshold of 0.0005."
    ShouldModel.prototype.notGlitch = function (threshold) {
        this._checkNaN(threshold, 'EXPECTED');

        for (var i = 1; i < this.target.length; i++) {
            var diff = Math.abs(this.target[i-1] - this.target[i]);
            if (diff >= threshold) {
                this._testFailed('has a glitch at index ' + i + ' of size ' + diff);
                return this._success;
            }
        }
        this._testPassed('has no glitch above the threshold of ' + threshold);
        return this._success;
    };

    // Check if the target promise is resolved correctly.
    //
    // Example:
    // Should('My promise', promise).beResolved().then(nextStuff);
    // Result:
    // "PASS My promise resolved correctly."
    // "FAIL My promise rejected incorrectly (with _ERROR_)."
    ShouldModel.prototype.beResolved = function () {
        return this.target.then(function () {
            this._testPassed('resolved correctly');
        }.bind(this), function (err) {
            this._testFailed('rejected incorrectly (with ' + err + ')');
        }.bind(this));
    };

    // Check if the target promise is rejected correctly.
    //
    // Example:
    // Should('My promise', promise).beRejected().then(nextStuff);
    // Result:
    // "PASS My promise rejected correctly (with _ERROR_)."
    // "FAIL My promise resolved incorrectly."
    ShouldModel.prototype.beRejected = function () {
        return this.target.then(function () {
            this._testFailed('resolved incorrectly');
        }.bind(this), function (err) {
            this._testPassed('rejected correctly (with ' + err + ')');
        }.bind(this));
    };

    // A summary message
    //
    // Example:
    // Should("Summary1", true).summarize("passed1", "failed1");
    // Should("Summary2", false).summarize("passed2", "failed2");
    // Result:
    // "PASS Summary1: passed1."
    // "FAIL Summary2: failed2."
    ShouldModel.prototype.summarize = function (pass, fail) {
        // It's really nice to have blank lines after the summary, but
        // testharness thinks the whole testsuite fails if we do that.
        if (this.target)
            this._testPassed(pass, false);
        else
            this._testFailed(fail, false);
        return this._success;
    }

    // Should() method.
    //
    // |desc| is the description of the task or check and |target| is a value
    // needs to be checked or a task to be performed. |opt| contains options for
    // printing out log messages: options are |opt.numberOfErrorLog| and
    // |opts.numberOfArrayLog|.
    return function (desc, target, opts) {
        var _opts = {
            numberOfErrorLog: 8,
            numberOfArrayLog: 16,
            verbose: true
        };

        if (opts instanceof Object) {
            if (opts.hasOwnProperty('numberOfErrorLog'))
                _opts.numberOfErrorLog = opts.numberOfErrorLog;
            if (opts.hasOwnProperty('numberOfArrayLog'))
                _opts.numberOfArrayLog = opts.numberOfArrayLog;
            if (opts.hasOwnProperty('brief'))
                _opts.brief = opts.brief;
            if (opts.hasOwnProperty('precision'))
                _opts.precision = opts.precision;
        }

        return new ShouldModel(desc, target, _opts);
    };

})();
