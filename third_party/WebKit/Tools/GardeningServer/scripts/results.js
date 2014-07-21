/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var results = results || {};

(function() {

var PASS = 'PASS';
var TIMEOUT = 'TIMEOUT';
var TEXT = 'TEXT';
var CRASH = 'CRASH';
var IMAGE = 'IMAGE';
var IMAGE_TEXT = 'IMAGE+TEXT';
var AUDIO = 'AUDIO';
var MISSING = 'MISSING';

var kFailingResults = [TEXT, IMAGE_TEXT, AUDIO];

var kExpectedImageSuffix = '-expected.png';
var kActualImageSuffix = '-actual.png';
var kImageDiffSuffix = '-diff.png';
var kExpectedAudioSuffix = '-expected.wav';
var kActualAudioSuffix = '-actual.wav';
var kExpectedTextSuffix = '-expected.txt';
var kActualTextSuffix = '-actual.txt';
var kDiffTextSuffix = '-diff.txt';
var kCrashLogSuffix = '-crash-log.txt';

var kPreferredSuffixOrder = [
    kExpectedImageSuffix,
    kActualImageSuffix,
    kImageDiffSuffix,
    kExpectedTextSuffix,
    kActualTextSuffix,
    kDiffTextSuffix,
    kCrashLogSuffix,
    kExpectedAudioSuffix,
    kActualAudioSuffix,
];

// Kinds of results.
results.kActualKind = 'actual';
results.kExpectedKind = 'expected';
results.kDiffKind = 'diff';
results.kUnknownKind = 'unknown';

// Types of tests.
results.kImageType = 'image';
results.kAudioType = 'audio';
results.kTextType = 'text';

function possibleSuffixListFor(failureTypeList)
{
    var suffixList = [];

    function pushImageSuffixes()
    {
        suffixList.push(kExpectedImageSuffix);
        suffixList.push(kActualImageSuffix);
        suffixList.push(kImageDiffSuffix);
    }

    function pushAudioSuffixes()
    {
        suffixList.push(kExpectedAudioSuffix);
        suffixList.push(kActualAudioSuffix);
    }

    function pushTextSuffixes()
    {
        suffixList.push(kActualTextSuffix);
        suffixList.push(kExpectedTextSuffix);
        suffixList.push(kDiffTextSuffix);
    }

    failureTypeList.forEach(function(failureType) {
        switch(failureType) {
        case IMAGE:
            pushImageSuffixes();
            break;
        case TEXT:
            pushTextSuffixes();
            break;
        case AUDIO:
            pushAudioSuffixes();
            break;
        case IMAGE_TEXT:
            pushImageSuffixes();
            pushTextSuffixes();
            break;
        case CRASH:
            suffixList.push(kCrashLogSuffix);
            break;
        case MISSING:
            pushImageSuffixes();
            pushTextSuffixes();
            break;
        default:
            // FIXME: Add support for the rest of the result types.
            // '-expected.html',
            // '-expected-mismatch.html',
            // ... and possibly more.
            break;
        }
    });

    return suffixList.unique();
}

function failureTypeList(failureBlob)
{
    return failureBlob.split(' ');
};

function resultsDirectoryURL(builderName)
{
    return config.layoutTestResultsURL + '/' + builderName.replace(/[ .()]/g, '_') + '/results/layout-test-results/';
}

results.failureInfo = function(testName, builderName, result)
{
    return {
        'testName': testName,
        'builderName': builderName,
        'failureTypeList': failureTypeList(result),
    };
}

results.resultKind = function(url)
{
    if (/-actual\.[a-z]+$/.test(url))
        return results.kActualKind;
    else if (/-expected\.[a-z]+$/.test(url))
        return results.kExpectedKind;
    else if (/diff\.[a-z]+$/.test(url))
        return results.kDiffKind;
    return results.kUnknownKind;
}

results.resultType = function(url)
{
    if (/\.png$/.test(url))
        return results.kImageType;
    if (/\.wav$/.test(url))
        return results.kAudioType;
    return results.kTextType;
}

function sortResultURLsBySuffix(urls)
{
    var sortedURLs = [];
    kPreferredSuffixOrder.forEach(function(suffix) {
        urls.forEach(function(url) {
            if (!url.endsWith(suffix))
                return;
            sortedURLs.push(url);
        });
    });
    if (sortedURLs.length != urls.length)
        throw "sortResultURLsBySuffix failed to return the same number of URLs.";
    return sortedURLs;
}

results._trimExtension = function(url)
{
    var index = url.lastIndexOf('.');
    if (index == -1)
        return url;
    return url.substr(0, index);
}

results.fetchResultsURLs = function(failureInfo)
{
    var testNameStem = results._trimExtension(failureInfo.testName);
    var urlStem = resultsDirectoryURL(failureInfo.builderName);

    var suffixList = possibleSuffixListFor(failureInfo.failureTypeList);
    var resultURLs = [];
    var probePromises = [];
    suffixList.forEach(function(suffix) {
        var url = urlStem + testNameStem + suffix;
        probePromises.push(net.probe(url).then(
            function() {
                resultURLs.push(url);
            },
            function() {}));
    });
    return Promise.all(probePromises).then(function() {
        return sortResultURLsBySuffix(resultURLs);
    });
};

})();
