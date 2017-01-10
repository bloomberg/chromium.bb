// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileOverview  This file includes legacy utility functions for the layout
 *                test.
 */


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

// Compare two arrays (commonly extracted from buffer.getChannelData()) with
// constraints:
//   options.thresholdSNR: Minimum allowed SNR between the actual and expected
//     signal. The default value is 10000.
//   options.thresholdDiffULP: Maximum allowed difference between the actual
//     and expected signal in ULP(Unit in the last place). The default is 0.
//   options.thresholdDiffCount: Maximum allowed number of sample differences
//     which exceeds the threshold. The default is 0.
//   options.bitDepth: The expected result is assumed to come from an audio
//     file with this number of bits of precision. The default is 16.
function compareBuffersWithConstraints(actual, expected, options) {
    if (!options)
        options = {};

    if (actual.length !== expected.length)
        testFailed('Buffer length mismatches.');

    var maxError = -1;
    var diffCount = 0;
    var errorPosition = -1;
    var thresholdSNR = (options.thresholdSNR || 10000);

    var thresholdDiffULP = (options.thresholdDiffULP || 0);
    var thresholdDiffCount = (options.thresholdDiffCount || 0);

    // By default, the bit depth is 16.
    var bitDepth = (options.bitDepth || 16);
    var scaleFactor = Math.pow(2, bitDepth - 1);

    var noisePower = 0, signalPower = 0;

    for (var i = 0; i < actual.length; i++) {
        var diff = actual[i] - expected[i];
        noisePower += diff * diff;
        signalPower += expected[i] * expected[i];

        if (Math.abs(diff) > maxError) {
            maxError = Math.abs(diff);
            errorPosition = i;
        }

        // The reference file is a 16-bit WAV file, so we will almost never get
        // an exact match between it and the actual floating-point result.
        if (Math.abs(diff) > scaleFactor)
            diffCount++;
    }

    var snr = 10 * Math.log10(signalPower / noisePower);
    var maxErrorULP = maxError * scaleFactor;

    Should("SNR", snr).beGreaterThanOrEqualTo(thresholdSNR);

    Should('Maximum difference (in ulp units (' + bitDepth + '-bits))',
      maxErrorULP).beLessThanOrEqualTo(thresholdDiffULP);

    Should('Number of differences between results', diffCount)
      .beLessThanOrEqualTo(thresholdDiffCount);
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

// Create an AudioBuffer of length |sampleFrameLength| having a constant value |constantValue|. If
// |constantValue| is a number, the buffer has one channel filled with that value. If
// |constantValue| is an array, the buffer is created wit a number of channels equal to the length
// of the array, and channel k is filled with the k'th element of the |constantValue| array.
function createConstantBuffer(context, sampleFrameLength, constantValue) {
    var channels;
    var values;

    if (typeof constantValue === "number") {
        channels = 1;
        values = [constantValue];
    } else {
        channels = constantValue.length;
        values = constantValue;
    }

    var audioBuffer = context.createBuffer(channels, sampleFrameLength, context.sampleRate);
    var n = audioBuffer.length;

    for (var c = 0; c < channels; ++c) {
        var data = audioBuffer.getChannelData(c);
        for (var i = 0; i < n; ++i)
            data[i] = values[c];
    }

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

// Compute the (linear) signal-to-noise ratio between |actual| and
// |expected|.  The result is NOT in dB!  If the |actual| and
// |expected| have different lengths, the shorter length is used.
function computeSNR(actual, expected) {
    var signalPower = 0;
    var noisePower = 0;

    var length = Math.min(actual.length, expected.length);

    for (var k = 0; k < length; ++k) {
        var diff = actual[k] - expected[k];
        signalPower += expected[k] * expected[k];
        noisePower += diff * diff;
    }

    return signalPower / noisePower;
}
