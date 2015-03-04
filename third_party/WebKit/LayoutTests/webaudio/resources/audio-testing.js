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
            // debug('[Audit] Task: ' + this.queue[this.currentTask] + ' completed.');
            if (this.currentTask === this.queue.length - 1) {
                // debug('[Audit] All task finished.');
            } else {
                this.tasks[this.queue[++this.currentTask]](done);
            }
            return;
        }.bind(this);

        // Start task 0.
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

// Generate a string out of function. Useful when throwing an error/exception
// with a description. It creates strings from a function, then extract the
// function body and trim out leading and trailing white spaces.
function getTaskSummary(func) {
    var lines = func.toString().split('\n').slice(1, -1);
    lines = lines.map(function (str) { return str.trim(); });
    lines = lines.join(' ');
    return '"' + lines + '"';
}

// The name space for |Should| test utility. Dependencies: testPassed(),
// testFailed() from resources/js-test.js
var Should = {};

// Expect an exception to be thrown with a certain type.
Should.throwWithType = function (type, func) {
    var summary = getTaskSummary(func);
    try {
        func();
        testFailed(summary + ' should throw ' + type + '.');
    } catch (e) {
        if (e.name === type)
            testPassed(summary + ' threw exception ' + e.name + '.');
        else
            testFailed(summary + ' should throw ' + type + '. Threw exception ' + e.name + '.');
    }
};

// Expect not to throw an exception.
Should.notThrow = function (func) {
    var summary = getTaskSummary(func);
    try {
        func();
        testPassed(summary + ' did not throw exception.');
    } catch (e) {
        testFailed(summary + ' should not throw exception. Threw exception ' + e.name + '.');
    }
};


// Verify if the channelData array contains a single constant |value|.
Should.haveValueInChannel = function (value, channelData) {
    var mismatch = {};
    for (var i = 0; i < channelData.length; i++) {
        if (channelData[i] !== value) {
            mismatch[i] = value;
        }
    }

    var numberOfmismatches = Object.keys(mismatch).length;
    if (numberOfmismatches === 0) {
        testPassed('ChannelData has expected values (' + value + ').');
    } else {
        testFailed(numberOfmismatches + ' values in ChannelData are not equal to ' + value + ':');
        for (var index in mismatch) {
            console.log('[' + index + '] : ' + mismatch[index]);
        }
    }
};