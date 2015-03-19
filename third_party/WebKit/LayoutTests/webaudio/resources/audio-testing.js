if (window.testRunner)
    testRunner.overridePreference("WebKitWebAudioEnabled", "1");

function writeString(s, a, offset) {
    for (var i = 0; i < s.length; ++i) {
        a[offset + i] = s.charCodeAt(i);
    }
}

function writeInt16(n, a, offset) {
    n = Math.floor(n);

    var b1 = n & 255;
    var b2 = (n >> 8) & 255;

    a[offset + 0] = b1;
    a[offset + 1] = b2;
}

function writeInt32(n, a, offset) {
    n = Math.floor(n);
    var b1 = n & 255;
    var b2 = (n >> 8) & 255;
    var b3 = (n >> 16) & 255;
    var b4 = (n >> 24) & 255;

    a[offset + 0] = b1;
    a[offset + 1] = b2;
    a[offset + 2] = b3;
    a[offset + 3] = b4;
}

function writeAudioBuffer(audioBuffer, a, offset) {
    var n = audioBuffer.length;
    var channels = audioBuffer.numberOfChannels;

    for (var i = 0; i < n; ++i) {
        for (var k = 0; k < channels; ++k) {
            var buffer = audioBuffer.getChannelData(k);
            var sample = buffer[i] * 32768.0;

            // Clip samples to the limitations of 16-bit.
            // If we don't do this then we'll get nasty wrap-around distortion.
            if (sample < -32768)
                sample = -32768;
            if (sample > 32767)
                sample = 32767;

            writeInt16(sample, a, offset);
            offset += 2;
        }
    }
}

function createWaveFileData(audioBuffer) {
    var frameLength = audioBuffer.length;
    var numberOfChannels = audioBuffer.numberOfChannels;
    var sampleRate = audioBuffer.sampleRate;
    var bitsPerSample = 16;
    var byteRate = sampleRate * numberOfChannels * bitsPerSample/8;
    var blockAlign = numberOfChannels * bitsPerSample/8;
    var wavDataByteLength = frameLength * numberOfChannels * 2; // 16-bit audio
    var headerByteLength = 44;
    var totalLength = headerByteLength + wavDataByteLength;

    var waveFileData = new Uint8Array(totalLength);

    var subChunk1Size = 16; // for linear PCM
    var subChunk2Size = wavDataByteLength;
    var chunkSize = 4 + (8 + subChunk1Size) + (8 + subChunk2Size);

    writeString("RIFF", waveFileData, 0);
    writeInt32(chunkSize, waveFileData, 4);
    writeString("WAVE", waveFileData, 8);
    writeString("fmt ", waveFileData, 12);

    writeInt32(subChunk1Size, waveFileData, 16);      // SubChunk1Size (4)
    writeInt16(1, waveFileData, 20);                  // AudioFormat (2)
    writeInt16(numberOfChannels, waveFileData, 22);   // NumChannels (2)
    writeInt32(sampleRate, waveFileData, 24);         // SampleRate (4)
    writeInt32(byteRate, waveFileData, 28);           // ByteRate (4)
    writeInt16(blockAlign, waveFileData, 32);         // BlockAlign (2)
    writeInt32(bitsPerSample, waveFileData, 34);      // BitsPerSample (4)

    writeString("data", waveFileData, 36);
    writeInt32(subChunk2Size, waveFileData, 40);      // SubChunk2Size (4)

    // Write actual audio data starting at offset 44.
    writeAudioBuffer(audioBuffer, waveFileData, 44);

    return waveFileData;
}

function createAudioData(audioBuffer) {
    return createWaveFileData(audioBuffer);
}

function finishAudioTest(event) {
    var audioData = createAudioData(event.renderedBuffer);
    testRunner.setAudioData(audioData);
    testRunner.notifyDone();
}

// Create an impulse in a buffer of length sampleFrameLength
function createImpulseBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);

    for (var k = 0; k < n; ++k) {
        dataL[k] = 0;
    }
    dataL[0] = 1;

    return audioBuffer;
}

// Create a buffer of the given length with a linear ramp having values 0 <= x < 1.
function createLinearRampBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);

    for (var i = 0; i < n; ++i)
        dataL[i] = i / n;

    return audioBuffer;
}

// Create a buffer of the given length having a constant value.
function createConstantBuffer(context, sampleFrameLength, constantValue) {
    var audioBuffer = context.createBuffer(1, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);

    for (var i = 0; i < n; ++i)
        dataL[i] = constantValue;

    return audioBuffer;
}

// Create a stereo impulse in a buffer of length sampleFrameLength
function createStereoImpulseBuffer(context, sampleFrameLength) {
    var audioBuffer = context.createBuffer(2, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;
    var dataL = audioBuffer.getChannelData(0);
    var dataR = audioBuffer.getChannelData(1);

    for (var k = 0; k < n; ++k) {
        dataL[k] = 0;
        dataR[k] = 0;
    }
    dataL[0] = 1;
    dataR[0] = 1;

    return audioBuffer;
}

// Convert time (in seconds) to sample frames.
function timeToSampleFrame(time, sampleRate) {
    return Math.floor(0.5 + time * sampleRate);
}

// Compute the number of sample frames consumed by noteGrainOn with
// the specified |grainOffset|, |duration|, and |sampleRate|.
function grainLengthInSampleFrames(grainOffset, duration, sampleRate) {
    var startFrame = timeToSampleFrame(grainOffset, sampleRate);
    var endFrame = timeToSampleFrame(grainOffset + duration, sampleRate);

    return endFrame - startFrame;
}

// True if the number is not an infinity or NaN
function isValidNumber(x) {
    return !isNaN(x) && (x != Infinity) && (x != -Infinity);
}


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
 var Audit = (function () {

    'use strict';

    function Tasks() {
        this.tasks = {};
        this.queue = [];
        this.currentTask = 0;
    }

    Tasks.prototype.defineTask = function (taskName, taskFunc) {
        this.tasks[taskName] = taskFunc;
    };

    Tasks.prototype.runTasks = function () {
        for (var i = 0; i < arguments.length; i++) {
          this.queue[i] = arguments[i];
        }

        // done() callback from tasks. Increase the task index and call the
        // next task.
        var done = function () {
            if (this.currentTask !== this.queue.length - 1) {
                ++this.currentTask;
                // debug('>> TASK: ' + this.queue[this.currentTask]);
                this.tasks[this.queue[this.currentTask]](done);
            }
            return;
        }.bind(this);

        // Start task 0.
        // debug('>> TASK: ' + this.queue[this.currentTask]);
        this.tasks[this.queue[this.currentTask]](done);
    };

    return {
        createTaskRunner: function () {
            return new Tasks();
        }
    };

})();


// Create an AudioBuffer for test verification. Fill an incremental index value
// into the each channel in the buffer. The channel index is between 1 and
// |numChannels|. For example, a 4-channel buffer created by this function will
// contain values 1, 2, 3 and 4 for each channel respectively.
function createTestingAudioBuffer(context, numChannels, length) {
    var buffer = context.createBuffer(numChannels, length, context.sampleRate);
    for (var i = 1; i <= numChannels; i++) {
        var data = buffer.getChannelData(i-1);
        for (var j = 0; j < data.length; j++) {
            // Storing channel index into the channel buffer.
            data[j] = i;
        }
    }
    return buffer;
}


// |Should| JS layout test utility.
// Dependency: ../resources/js-test.js
var Should = (function () {

    'use strict';

    // ShouldModel internal class. For the exposed (factory) method, it is the
    // return value of this closure.
    function ShouldModel(desc, target, opts) {
        this.desc = desc;
        this.target = target;

        // If the number of errors is greater than this, the rest of error
        // messages are suppressed. the value is fairly arbitrary, but shouldn't
        // be too small or too large.
        this.NUM_ERRORS_LOG = opts.numberOfErrorLog;

        // If the number of array elements is greater than this, the rest of
        // elements will be omitted.
        this.NUM_ARRAY_LOG = opts.numberOfArrayLog;
    }

    // Internal methods starting with a underscore.
    ShouldModel.prototype._testPassed = function (msg) {
        testPassed(this.desc + ' ' + msg + '.');
    };

    ShouldModel.prototype._testFailed = function (msg) {
        testFailed(this.desc + ' ' + msg + '.');
    };

    ShouldModel.prototype._isArray = function (arg) {
        return arg instanceof Array || arg instanceof Float32Array;
    };

    ShouldModel.prototype._assert = function (expression, reason) {
        if (expression)
            return;

        var failureMessage = 'Assertion failed: ' + reason + ' ' + this.desc +'.';
        testFailed(failureMessage);
        throw failureMessage;
    };

    // Check if |target| is equal to |value|.
    //
    // Example:
    // Should('Zero', 0).beEqualTo(0);
    // Result:
    // "PASS Zero is equal to 0."
    ShouldModel.prototype.beEqualTo = function (value) {
        var type = typeof value;
        this._assert(type === 'number' || type === 'string',
            'value should be number or string for');

        if (this.target === value)
            this._testPassed('is equal to ' + value);
        else
            this._testFailed('was ' + value + ' instead of ' + this.target);
    };

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
                this._testPassed('threw an exception');
            else if (error.name === errorType)
                this._testPassed('threw ' + errorType);
            else
                this._testFailed('threw ' + error.name + ' instead of ' + exception);
        }
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
            this._testFailed('threw ' + error.name);
        }
    };

    // Check if |target| array is filled with constant values.
    //
    // Example:
    // Should('[2, 2, 2]', [2, 2, 2]).beConstantValueOf(2);
    // Result:
    // "PASS [2, 2, 2] has constant values of 2."
    ShouldModel.prototype.beConstantValueOf = function (value) {
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
    };

    // Check if |target| array is identical to |expected| array element-wise.
    //
    // Example:
    // Should('[1, 2, 3]', [1, 2, 3]).beEqualToArray([1, 2, 3]);
    // Result:
    // "PASS [1, 2, 3] is identical to the array [1,2,3]."
    ShouldModel.prototype.beEqualToArray = function (array) {
        this._assert(this._isArray(array) && this.target.length === array.length,
            'Invalid array or the length does not match.');

        var mismatches = {};
        for (var i = 0; i < this.target.length; i++) {
            if (this.target[i] !== array[i])
                mismatches[i] = this.target[i];
        }

        var numberOfmismatches = Object.keys(mismatches).length;
        var arrStr = (array.length > this.NUM_ARRAY_LOG) ?
        array.slice(0, this.NUM_ARRAY_LOG).toString() + '...' : array.toString();

        if (numberOfmismatches === 0) {
            this._testPassed('is identical to the array [' + arrStr + ']');
        } else {
            var counter = 0;
            var failureMessage = 'is not equal to the array [' + arrStr + ']';
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
    };

    // Check if |target| array contains a set of values in a certain order.
    //
    // Example:
    // Should('My random array', [1, 1, 3, 3, 2]).containValues([1, 3, 2]);
    // Result:
    // "PASS My random array contains all the expected values in the correct
    //  order: [1,3,2]."
    ShouldModel.prototype.containValues = function (expected) {
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
    };

    // Should() method.
    //
    // |desc| is the description of the task or check and |target| is a value
    // needs to be checked or a task to be performed. |opt| contains options for
    // printing out log messages: options are |opt.numberOfErrorLog| and
    // |opts.numberOfArrayLog|.
    return function (desc, target, opts) {
        var _opts = {
            numberOfErrorLog: 8,
            numberOfArrayLog: 16
        };

        if (opts instanceof Object) {
            if (opts.hasOwnProperty('numberOfErrorLog'))
                _opts.numberOfErrorLog = opts.numberOfErrorLog;
            if (opts.hasOwnProperty('numberOfArrayLog'))
                _opts.numberOfArrayLog = opts.numberOfArrayLog;
        }

        return new ShouldModel(desc, target, _opts);
    };

})();