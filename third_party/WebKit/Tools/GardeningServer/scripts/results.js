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

var kResultsName = 'failing_results.json';

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

var kPNGExtension = 'png';
var kTXTExtension = 'txt';
var kWAVExtension = 'wav';

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
    // FIXME: Add support for the rest of the result types.
];

// Kinds of results.
results.kActualKind = 'actual';
results.kExpectedKind = 'expected';
results.kDiffKind = 'diff';
results.kUnknownKind = 'unknown';

// Types of tests.
results.kImageType = 'image'
results.kAudioType = 'audio'
results.kTextType = 'text'
// FIXME: There are more types of tests.

function layoutTestResultsURL(platform)
{
    return config.kPlatforms[platform].layoutTestResultsURL;
}

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
        // '-wdiff.html',
        // '-pretty-diff.html',
    }

    $.each(failureTypeList, function(index, failureType) {
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

    return base.uniquifyArray(suffixList);
}

results.failureTypeToExtensionList = function(failureType)
{
    switch(failureType) {
    case IMAGE:
        return [kPNGExtension];
    case AUDIO:
        return [kWAVExtension];
    case TEXT:
        return [kTXTExtension];
    case MISSING:
    case IMAGE_TEXT:
        return [kTXTExtension, kPNGExtension];
    default:
        // FIXME: Add support for the rest of the result types.
        // '-expected.html',
        // '-expected-mismatch.html',
        // ... and possibly more.
        return [];
    }
};

results.failureTypeList = function(failureBlob)
{
    return failureBlob.split(' ');
};

results.canRebaseline = function(failureTypeList)
{
    return failureTypeList.some(function(element) {
        return results.failureTypeToExtensionList(element).length > 0;
    });
};

results.directoryForBuilder = function(builderName)
{
    return config.kPlatforms[config.currentPlatform].resultsDirectoryNameFromBuilderName(builderName);
}

function resultsDirectoryURL(platform, builderName)
{
    if (config.useLocalResults)
        return '/localresult?path=';
    return layoutTestResultsURL(platform) + '/' + results.directoryForBuilder(builderName) + '/results/layout-test-results/';
}

function resultsPrefixListingURL(platform, builderName)
{
    return layoutTestResultsURL(platform) + '/?prefix=' + results.directoryForBuilder(builderName) + '/&delimiter=/';
}

function resultsDirectoryURLForBuildNumber(platform, builderName, buildNumber)
{
    return layoutTestResultsURL(platform) + '/' + results.directoryForBuilder(builderName) + '/' + buildNumber + '/' ;
}

function resultsSummaryURL(platform, builderName)
{
    return resultsDirectoryURL(platform, builderName) + kResultsName;
}

function resultsSummaryURLForBuildNumber(platform, builderName, buildNumber)
{
    return resultsDirectoryURLForBuildNumber(platform, builderName, buildNumber) + kResultsName;
}

var g_resultsCache = new base.AsynchronousCache(function (key, callback) {
    net.jsonp(key, callback);
});

results.ResultAnalyzer = base.extends(Object, {
    init: function(resultNode)
    {
        this._isUnexpected = resultNode.is_unexpected;
        this._actual = resultNode ? results.failureTypeList(resultNode.actual) : [];
        this._expected = resultNode ? this._addImpliedExpectations(results.failureTypeList(resultNode.expected)) : [];
    },
    _addImpliedExpectations: function(resultsList)
    {
        if (resultsList.indexOf('FAIL') == -1)
            return resultsList;
        return resultsList.concat(kFailingResults);
    },
    _hasPass: function(results)
    {
        return results.indexOf(PASS) != -1;
    },
    unexpectedResults: function()
    {
        return this._actual.filter(function(result) {
            return this._expected.indexOf(result) == -1;
        }, this);
    },
    succeeded: function()
    {
        return this._hasPass(this._actual);
    },
    flaky: function()
    {
        return this._actual.length > 1;
    },
    wontfix: function()
    {
        return this._expected.indexOf('WONTFIX') != -1;
    },
    hasUnexpectedFailures: function()
    {
        return this._isUnexpected;
    }
})

function isExpectedFailure(resultNode)
{
    var analyzer = new results.ResultAnalyzer(resultNode);
    return !analyzer.hasUnexpectedFailures() && !analyzer.succeeded() && !analyzer.flaky() && !analyzer.wontfix();
}

function isUnexpectedFailure(resultNode)
{
    var analyzer = new results.ResultAnalyzer(resultNode);
    return analyzer.hasUnexpectedFailures() && !analyzer.succeeded() && !analyzer.flaky() && !analyzer.wontfix();
}

function isResultNode(node)
{
    return !!node.actual;
}

results.expectedFailures = function(resultsTree)
{
    return base.filterTree(resultsTree.tests, isResultNode, isExpectedFailure);
};

results.unexpectedFailures = function(resultsTree)
{
    return base.filterTree(resultsTree.tests, isResultNode, isUnexpectedFailure);
};

function resultsByTest(resultsByBuilder, filter)
{
    var resultsByTest = {};

    $.each(resultsByBuilder, function(builderName, resultsTree) {
        $.each(filter(resultsTree), function(testName, resultNode) {
            resultsByTest[testName] = resultsByTest[testName] || {};
            resultsByTest[testName][builderName] = resultNode;
        });
    });

    return resultsByTest;
}

results.expectedFailuresByTest = function(resultsByBuilder)
{
    return resultsByTest(resultsByBuilder, results.expectedFailures);
};

results.unexpectedFailuresByTest = function(resultsByBuilder)
{
    return resultsByTest(resultsByBuilder, results.unexpectedFailures);
};

results.failureInfoForTestAndBuilder = function(resultsByTest, testName, builderName)
{
    var failureInfoForTest = {
        'testName': testName,
        'builderName': builderName,
        'failureTypeList': results.failureTypeList(resultsByTest[testName][builderName].actual),
    };

    return failureInfoForTest;
};

results.collectUnexpectedResults = function(dictionaryOfResultNodes)
{
    var collectedResults = [];
    $.each(dictionaryOfResultNodes, function(key, resultNode) {
        var analyzer = new results.ResultAnalyzer(resultNode);
        collectedResults = collectedResults.concat(analyzer.unexpectedResults());
    });
    return base.uniquifyArray(collectedResults);
};

// Callback data is [{ buildNumber:, url: }]
function historicalResultsLocations(platform, builderName, callback)
{
    var listingURL = resultsPrefixListingURL(platform, builderName);
    net.get(listingURL, function(prefixListingDocument) {
        var historicalResultsData = [];
        $(prefixListingDocument).find("Prefix").each(function() {
            var buildString = this.textContent.replace(results.directoryForBuilder(builderName) + '/', '');
            if (buildString.match(/\d+\//)) {
                var buildNumber = parseInt(buildString);
                var resultsData = {
                    'buildNumber': buildNumber,
                    'url': resultsSummaryURLForBuildNumber(platform, builderName, buildNumber)
                };
                historicalResultsData.unshift(resultsData);
           }
        });
        callback(historicalResultsData);
    });
}

function walkHistory(platform, builderName, testName, callback)
{
    var indexOfNextKeyToFetch = 0;
    var keyList = [];

    function continueWalk()
    {
        if (indexOfNextKeyToFetch >= keyList.length) {
            processResultNode(0, null);
            return;
        }

        var resultsURL = keyList[indexOfNextKeyToFetch].url;
        ++indexOfNextKeyToFetch;
        g_resultsCache.get(resultsURL, function(resultsTree) {
            if ($.isEmptyObject(resultsTree)) {
                continueWalk();
                return;
            }
            var resultNode = results.resultNodeForTest(resultsTree, testName);
            var revision = parseInt(resultsTree['blink_revision'])
            if (isNaN(revision))
                revision = 0;
            processResultNode(revision, resultNode);
        });
    }

    function processResultNode(revision, resultNode)
    {
        var shouldContinue = callback(revision, resultNode);
        if (!shouldContinue)
            return;
        continueWalk();
    }

    historicalResultsLocations(platform, builderName, function(resultsLocations) {
        keyList = resultsLocations;
        continueWalk();
    });
}

results.regressionRangeForFailure = function(builderName, testName, callback)
{
    var oldestFailingRevision = 0;
    var newestPassingRevision = 0;

    // FIXME: should treat {platform, builderName} as a tuple
    walkHistory(config.currentPlatform, builderName, testName, function(revision, resultNode) {
        if (!revision) {
            callback(oldestFailingRevision, newestPassingRevision);
            return false;
        }
        if (!resultNode) {
            newestPassingRevision = revision;
            callback(oldestFailingRevision, newestPassingRevision);
            return false;
        }
        if (isUnexpectedFailure(resultNode)) {
            oldestFailingRevision = revision;
            return true;
        }
        if (!oldestFailingRevision)
            return true;  // We need to keep looking for a failing revision.
        newestPassingRevision = revision;
        callback(oldestFailingRevision, newestPassingRevision);
        return false;
    });
};

function mergeRegressionRanges(regressionRanges)
{
    var mergedRange = {};

    mergedRange.oldestFailingRevision = 0;
    mergedRange.newestPassingRevision = 0;

    $.each(regressionRanges, function(builderName, range) {
        if (!range.oldestFailingRevision && !range.newestPassingRevision)
            return

        if (!mergedRange.oldestFailingRevision)
            mergedRange.oldestFailingRevision = range.oldestFailingRevision;
        if (!mergedRange.newestPassingRevision)
            mergedRange.newestPassingRevision = range.newestPassingRevision;

        if (range.oldestFailingRevision && range.oldestFailingRevision < mergedRange.oldestFailingRevision)
            mergedRange.oldestFailingRevision = range.oldestFailingRevision;
        if (range.newestPassingRevision > mergedRange.newestPassingRevision)
            mergedRange.newestPassingRevision = range.newestPassingRevision;
    });

    return mergedRange;
}

results.unifyRegressionRanges = function(builderNameList, testName, callback)
{
    var regressionRanges = {};

    var tracker = new base.RequestTracker(builderNameList.length, function() {
        var mergedRange = mergeRegressionRanges(regressionRanges);
        callback(mergedRange.oldestFailingRevision, mergedRange.newestPassingRevision);
    });

    $.each(builderNameList, function(index, builderName) {
        results.regressionRangeForFailure(builderName, testName, function(oldestFailingRevision, newestPassingRevision) {
            var range = {};
            range.oldestFailingRevision = oldestFailingRevision;
            range.newestPassingRevision = newestPassingRevision;
            regressionRanges[builderName] = range;
            tracker.requestComplete();
        });
    });
};

results.resultNodeForTest = function(resultsTree, testName)
{
    var testNamePath = testName.split('/');
    var currentNode = resultsTree['tests'];
    $.each(testNamePath, function(index, segmentName) {
        if (!currentNode)
            return;
        currentNode = (segmentName in currentNode) ? currentNode[segmentName] : null;
    });
    return currentNode;
};

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
    $.each(kPreferredSuffixOrder, function(i, suffix) {
        $.each(urls, function(j, url) {
            if (!base.endsWith(url, suffix))
                return;
            sortedURLs.push(url);
        });
    });
    if (sortedURLs.length != urls.length)
        throw "sortResultURLsBySuffix failed to return the same number of URLs."
    return sortedURLs;
}

results.fetchResultsURLs = function(failureInfo, callback)
{
    var testNameStem = base.trimExtension(failureInfo.testName);
    var urlStem = resultsDirectoryURL(config.currentPlatform, failureInfo.builderName);

    var suffixList = possibleSuffixListFor(failureInfo.failureTypeList);
    var resultURLs = [];
    var tracker = new base.RequestTracker(suffixList.length, function() {
        callback(sortResultURLsBySuffix(resultURLs));
    });
    $.each(suffixList, function(index, suffix) {
        var url = urlStem + testNameStem + suffix;
        net.probe(url, {
            success: function() {
                resultURLs.push(url);
                tracker.requestComplete();
            },
            error: function() {
                tracker.requestComplete();
            },
        });
    });
};

results.fetchResultsByBuilder = function(builderNameList, callback)
{
    var resultsByBuilder = {};
    var tracker = new base.RequestTracker(builderNameList.length, function() {
        callback(resultsByBuilder);
    });
    $.each(builderNameList, function(index, builderName) {
        var resultsURL = resultsSummaryURL(config.currentPlatform, builderName);
        net.jsonp(resultsURL, function(resultsTree) {
            resultsByBuilder[builderName] = resultsTree;
            tracker.requestComplete();
        });
    });
};

})();
