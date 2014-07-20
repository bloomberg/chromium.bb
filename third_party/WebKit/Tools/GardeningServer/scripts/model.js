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

var model = model || {};

(function () {

var kCommitLogLength = 50;

model.state = {};
model.state.failureAnalysisByTest = {};

function findAndMarkRevertedRevisions(commitDataList)
{
    var revertedRevisions = {};
    Object.keys(commitDataList, function(index, commitData) {
        if (commitData.revertedRevision)
            revertedRevisions[commitData.revertedRevision] = true;
    });
    Object.keys(commitDataList, function(index, commitData) {
        if (commitData.revision in revertedRevisions)
            commitData.wasReverted = true;
    });
}

function fuzzyFind(testName, commitData)
{
    var indexOfLastDot = testName.lastIndexOf('.');
    var stem = indexOfLastDot == -1 ? testName : testName.substr(0, indexOfLastDot);
    return commitData.message.indexOf(stem) != -1;
}

function heuristicallyNarrowRegressionRange(failureAnalysis)
{
    var commitDataList = model.state.recentCommits;
    var commitDataIndex = commitDataList.length - 1;

    for(var revision = failureAnalysis.newestPassingRevision + 1; revision <= failureAnalysis.oldestFailingRevision; ++revision) {
        while (commitDataIndex >= 0 && commitDataList[commitDataIndex].revision < revision)
            --commitDataIndex;
        var commitData = commitDataList[commitDataIndex];
        if (commitData.revision != revision)
            continue;
        if (fuzzyFind(failureAnalysis.testName, commitData)) {
            failureAnalysis.oldestFailingRevision = revision;
            failureAnalysis.newestPassingRevision = revision - 1;
            return;
        }
    }
}

var g_commitIndex = {};

model.updateRecentCommits = function()
{
    return trac.recentCommitData('trunk', kCommitLogLength).then(function(commitDataList) {
        model.state.recentCommits = commitDataList;
        updateCommitIndex();
        findAndMarkRevertedRevisions(model.state.recentCommits);
    });
};

function updateCommitIndex()
{
    model.state.recentCommits.forEach(function(commitData) {
        g_commitIndex[commitData.revision] = commitData;
    });
}

model.commitDataListForRevisionRange = function(fromRevision, toRevision)
{
    var result = [];
    for (var revision = fromRevision; revision <= toRevision; ++revision) {
        var commitData = g_commitIndex[revision];
        if (commitData)
            result.push(commitData);
    }
    return result;
};

model.buildersInFlightForRevision = function(revision)
{
    var builders = {};
    Object.keys(model.state.resultsByBuilder).forEach(function(builderName) {
        var results = model.state.resultsByBuilder[builderName];
        if (parseInt(results.blink_revision) < revision)
            builders[builderName] = { actual: 'BUILDING' };
    });
    return builders;
};

model.updateResultsByBuilder = function()
{
    return results.fetchResultsByBuilder(Object.keys(config.builders)).then(function(resultsByBuilder) {
        model.state.resultsByBuilder = resultsByBuilder;
    });
};

// failureCallback is called multiple times: once for each failure
model.analyzeUnexpectedFailures = function(failureCallback)
{
    var unexpectedFailures = results.unexpectedFailuresByTest(model.state.resultsByBuilder);

    Object.keys(model.state.failureAnalysisByTest, function(testName, failureAnalysis) {
        if (!(testName in unexpectedFailures))
            delete model.state.failureAnalysisByTest[testName];
    });

    var failurePromises = [];
    Object.keys(unexpectedFailures, function(testName, resultNodesByBuilder) {
        var builderNameList = Object.keys(resultNodesByBuilder);
        failurePromises.push(results.unifyRegressionRanges(builderNameList, testName).then(function(result) {
            var oldestFailingRevision = result[0];
            var newestPassingRevision = result[1];
            var failureAnalysis = {
                'testName': testName,
                'resultNodesByBuilder': resultNodesByBuilder,
                'oldestFailingRevision': oldestFailingRevision,
                'newestPassingRevision': newestPassingRevision,
            };

            heuristicallyNarrowRegressionRange(failureAnalysis);

            var previousFailureAnalysis = model.state.failureAnalysisByTest[testName];
            if (previousFailureAnalysis
                && previousFailureAnalysis.oldestFailingRevision <= failureAnalysis.oldestFailingRevision
                && previousFailureAnalysis.newestPassingRevision >= failureAnalysis.newestPassingRevision) {
                failureAnalysis.oldestFailingRevision = previousFailureAnalysis.oldestFailingRevision;
                failureAnalysis.newestPassingRevision = previousFailureAnalysis.newestPassingRevision;
            }

            model.state.failureAnalysisByTest[testName] = failureAnalysis;

            failureCallback(failureAnalysis, failurePromises.length);
        }));
    });
    return Promise.all(failurePromises);
};

})();
