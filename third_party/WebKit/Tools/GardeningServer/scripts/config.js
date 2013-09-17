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

var config = config || {};

(function() {

config.kBuildNumberLimit = 20;

config.kPlatforms = {
    'chromium' : {
        label : 'Chromium',
        buildConsoleURL: 'http://build.chromium.org/p/chromium.webkit',

        layoutTestResultsURL: 'https://storage.googleapis.com/chromium-layout-test-archives',
        waterfallURL: 'http://build.chromium.org/p/chromium.webkit/waterfall',
        builders: {
            'WebKit XP': {version: 'xp'},
            'WebKit Win7': {version: 'win7'},
            'WebKit Win7 (dbg)': {version: 'win7', debug: true},
            'WebKit Linux': {version: 'lucid', is64bit: true},
            'WebKit Linux 32': {version: 'lucid'},
            'WebKit Linux (dbg)': {version: 'lucid', is64bit: true, debug: true},
            'WebKit Mac10.6': {version: 'snowleopard'},
            'WebKit Mac10.6 (dbg)': {version: 'snowleopard', debug: true},
            'WebKit Mac10.7': {version: 'lion'},
            'WebKit Mac10.7 (dbg)': {version: 'lion', debug: true},
            'WebKit Mac10.8': {version: 'mountainlion'},
            'WebKit Mac10.8 (retina)': {version: 'retina'},
        },
        resultsDirectoryNameFromBuilderName: function(builderName) {
            return base.underscoredBuilderName(builderName);
        },
        _builderApplies: function(builderName) {
            // FIXME: Should garden-o-matic show these?
            // WebKit Android and ASAN are red all the time.
            // Remove this function entirely once they are better supported.
            return builderName.indexOf('GPU') == -1 &&
                   builderName.indexOf('ASAN') == -1 &&
                   builderName.indexOf('WebKit (Content Shell) Android') == -1 &&
                   builderName.indexOf('WebKit Android') == -1;
        },
    },
};

config.currentPlatform = 'chromium';
config.kBlinkSvnURL = 'svn://svn.chromium.org/blink/trunk';
config.kBlinkRevisionURL = 'http://src.chromium.org/viewvc/blink';
config.kSvnLogURL = 'http://build.chromium.org/cgi-bin/svn-log';
config.kRietveldURL = "https://codereview.chromium.org";

var kTenMinutesInMilliseconds = 10 * 60 * 1000;
config.kUpdateFrequency = kTenMinutesInMilliseconds;
config.kRelativeTimeUpdateFrequency = 1000 * 60;

config.currentBuilders = function() {
    return config.kPlatforms[config.currentPlatform].builders;
};

config.builderApplies = function(builderName) {
    return config.kPlatforms[config.currentPlatform]._builderApplies(builderName);
};

config.useLocalResults = Boolean(base.getURLParameter('useLocalResults')) || false;

})();
