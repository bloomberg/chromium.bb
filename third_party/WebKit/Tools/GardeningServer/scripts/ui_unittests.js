/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

(function () {

module("ui");

var flakinessBaseUrl = 'http://test-results.appspot.com/dashboards/flakiness_dashboard.html#';

test('urlForFlakinessDashboard', 5, function() {
    equal(ui.urlForFlakinessDashboard('foo', 'bar', 'blink'),
        flakinessBaseUrl + 'group=@ToT%20Blink&tests=foo&testType=bar');
    equal(ui.urlForFlakinessDashboard('foo', 'bar', 'chromium'),
        flakinessBaseUrl + 'group=@ToT%20Chromium&tests=foo&testType=bar');
    equal(ui.urlForFlakinessDashboard(['foo', 'baz'], 'bar', 'blink'),
        flakinessBaseUrl + 'group=@ToT%20Blink&tests=foo%2Cbaz&testType=bar');
    equal(ui.urlForFlakinessDashboard('foo', 'webkit_tests', 'blink'),
        flakinessBaseUrl + 'group=@ToT%20Blink&tests=foo&testType=layout-tests');
    equal(ui.urlForFlakinessDashboard('foo', 'layout-tests', 'blink'),
        flakinessBaseUrl + 'group=@ToT%20Blink&tests=foo&testType=layout-tests');
});

test('urlForEmbeddedFlakinessDashboard', 1, function() {
    equal(ui.urlForEmbeddedFlakinessDashboard('foo', 'bar', 'blink'),
        flakinessBaseUrl + 'group=@ToT%20Blink&tests=foo&testType=bar&showChrome=false');
});

})();
