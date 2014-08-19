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

var unittest = unittest || {};

(function () {

module("results");

var MockResultsBaseURL = 'https://storage.googleapis.com/chromium-layout-test-archives/Mock_Builder/results/layout-test-results';

test("trimExtension", 6, function() {
    equals(results._trimExtension("xyz"), "xyz");
    equals(results._trimExtension("xy.z"), "xy");
    equals(results._trimExtension("x.yz"), "x");
    equals(results._trimExtension("x.y.z"), "x.y");
    equals(results._trimExtension(".xyz"), "");
    equals(results._trimExtension(""), "");
});

test("failureInfo", 1, function() {
    var failureInfo = results.failureInfo("userscripts/another-test.html", "Mock Builder", "FAIL PASS");
    deepEqual(failureInfo, {
        "testName": "userscripts/another-test.html",
        "builderName": "Mock Builder",
        "failureTypeList": ["FAIL", "PASS"],
    });
});

test("resultKind", 12, function() {
    equals(results.resultKind("http://example.com/foo-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo.bar-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo.bar-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo.bar-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo-actual.png"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.png"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.png"), "diff");
    equals(results.resultKind("http://example.com/foo-pretty-diff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-wdiff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-xyz.html"), "unknown");
});

test("resultType", 12, function() {
    equals(results.resultType("http://example.com/foo-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo-actual.png"), "image");
    equals(results.resultType("http://example.com/foo-expected.png"), "image");
    equals(results.resultType("http://example.com/foo-diff.png"), "image");
    equals(results.resultType("http://example.com/foo-pretty-diff.html"), "text");
    equals(results.resultType("http://example.com/foo-wdiff.html"), "text");
    equals(results.resultType("http://example.com/foo.xyz"), "text");
});

asyncTest("fetchResultsURLs", 6, function() {
    var simulator = new NetworkSimulator(ok, start);

    var probedURLs = [];
    simulator.probe = function(url)
    {
        probedURLs.push(url);
        if (url.endsWith('.txt'))
            return Promise.resolve();
        else if (/taco.+png$/.test(url))
            return Promise.resolve();
        else
            return Promise.reject();
    };

    simulator.runTest(function() {
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/another-test.html",
            'failureTypeList': ['IMAGE', 'CRASH'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, [
                MockResultsBaseURL + "/userscripts/another-test-crash-log.txt"
            ]);
        });
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/another-test.html",
            'failureTypeList': ['TIMEOUT'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, []);
        });
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/taco.html",
            'failureTypeList': ['IMAGE', 'IMAGE+TEXT'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, [
                MockResultsBaseURL + "/userscripts/taco-expected.png",
                MockResultsBaseURL + "/userscripts/taco-actual.png",
                MockResultsBaseURL + "/userscripts/taco-diff.png",
                MockResultsBaseURL + "/userscripts/taco-expected.txt",
                MockResultsBaseURL + "/userscripts/taco-actual.txt",
                MockResultsBaseURL + "/userscripts/taco-diff.txt",
            ]);
        });
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/another-test.html",
            'failureTypeList': ['LEAK'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, [
                MockResultsBaseURL + "/userscripts/another-test-leak-log.txt"
            ]);
        });
    }).then(function() {
        deepEqual(probedURLs, [
            MockResultsBaseURL + "/userscripts/another-test-expected.png",
            MockResultsBaseURL + "/userscripts/another-test-actual.png",
            MockResultsBaseURL + "/userscripts/another-test-diff.png",
            MockResultsBaseURL + "/userscripts/another-test-crash-log.txt",
            MockResultsBaseURL + "/userscripts/taco-expected.png",
            MockResultsBaseURL + "/userscripts/taco-actual.png",
            MockResultsBaseURL + "/userscripts/taco-diff.png",
            MockResultsBaseURL + "/userscripts/taco-actual.txt",
            MockResultsBaseURL + "/userscripts/taco-expected.txt",
            MockResultsBaseURL + "/userscripts/taco-diff.txt",
            MockResultsBaseURL + "/userscripts/another-test-leak-log.txt",
        ]);
        start();
    });
});

})();
