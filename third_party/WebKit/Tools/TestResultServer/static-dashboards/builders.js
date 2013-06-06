// Copyright (C) 2012 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// @fileoverview File that lists builders, their masters, and logical groupings
// of them.

function LOAD_BUILDBOT_DATA(builderData)
{
    builders.masters = {};
    builderData['masters'].forEach(function(master) {
        builders.masters[master.name] = new builders.BuilderMaster(master.name, master.url, master.tests, master.groups);
    });
    builders.groups = builderData['groups'];
}

var builders = builders || {};

(function() {

// FIXME: Move some of this loading logic into loader.js.

builders._currentBuilderGroup = {};

// FIXME: this list should be built on demand.
builders._GROUP_NAMES = [
    '@DEPS - chromium.org',
    '@DEPS CrOS - chromium.org',
    '@DEPS FYI - chromium.org',
    '@ToT - chromium.org',
    'Content Shell @ToT - chromium.org',
]

builders.getBuilderGroup = function(groupName, testType)
{
    if (!builders in builders._currentBuilderGroup) {
        builders._currentBuilderGroup = builders.loadBuildersList(groupName, testType);
    }
    return builders._currentBuilderGroup;
}

builders.loadBuildersList = function(groupName, testType)
{
    if (!groupName || !testType) {
        console.warn("Group name and/or test type were empty.");
        return new BuilderGroup(false);
    }
    var builderGroup = new BuilderGroup(isToTBlink(groupName));

    for (masterName in builders.masters) {
        if (!builders.masters[masterName])
            continue;

        var master = builders.masters[masterName];
        var hasTest = testType in master.tests;
        var isInGroup = master.groups.indexOf(groupName) != -1;

        if (hasTest && isInGroup) {
            var builderList = master.tests[testType].builders;
            var builderFilter = selectBuilderFilter(groupName, testType);
            if (builderFilter)
                builderList = builderList.filter(builderFilter);
            builderGroup.append(builderList);
        }
    }

    populateBuilderToMaster();

    builders._currentBuilderGroup = builderGroup;
    return builders._currentBuilderGroup;
}

builders.getAllGroupNames = function()
{
    return builders._GROUP_NAMES;
}

builders.BuilderMaster = function(name, basePath, tests, groups)
{
    this.name = name;
    this.basePath = basePath;
    this.tests = tests;
    this.groups = groups;
}

builders.BuilderMaster.prototype = {
    logPath: function(builder, buildNumber)
    {
        return this.basePath + '/builders/' + builder + '/builds/' + buildNumber;
    },
    builderJsonPath: function()
    {
        return this.basePath + '/json/builders';
    },
}

})();

// FIXME: Move everything below into the anonymous namespace above.
function BuilderGroup(isToTBlink)
{
    this.isToTBlink = isToTBlink;
    // Map of builderName (the name shown in the waterfall) to builderPath (the
    // path used in the builder's URL)
    this.builders = {};
}

BuilderGroup.prototype.append = function(builders) {
    builders.forEach(function(builderName) {
        this.builders[builderName] = builderName.replace(/[ .()]/g, '_');
    }, this);
};

BuilderGroup.prototype.defaultBuilder = function()
{
    for (var builder in this.builders)
        return builder;
    console.error('There are no builders in this builder group.');
}

BuilderGroup.prototype.master = function()
{
    return builderMaster(this.defaultBuilder());
}

BuilderGroup.TOT_WEBKIT = true;
BuilderGroup.DEPS_WEBKIT = false;

var BUILDER_TO_MASTER = {};

function builderMaster(builderName)
{
    return BUILDER_TO_MASTER[builderName];
}

function isChromiumContentShellTestRunner(builder)
{
    return builder.indexOf('(Content Shell)') != -1;
}

function isChromiumWebkitTipOfTreeTestRunner(builder)
{
    // FIXME: Remove the Android check once the android tests bot is actually uploading results.
    return builder.indexOf('ASAN') == -1 &&
        builder.indexOf('Android') == -1 &&
        !isChromiumContentShellTestRunner(builder) &&
        !isChromiumWebkitDepsTestRunner(builder);
}

function isChromiumWebkitDepsTestRunner(builder)
{
    return builder.indexOf('(deps)') != -1;
}

// FIXME: replace this by looking up which bots run which test suites.
function selectBuilderFilter(groupName, testType)
{
    var builderFilter = null;
    switch (testType) {
    case 'layout-tests':
        switch (groupName) {
        case '@ToT - chromium.org':
            builderFilter = isChromiumWebkitTipOfTreeTestRunner;
            break;
        case 'Content Shell @ToT - chromium.org':
            builderFilter = isChromiumContentShellTestRunner;
            break;
        case '@DEPS - chromium.org':
            builderFilter = isChromiumWebkitDepsTestRunner;
            break;
        }
        break;

    case 'test_shell_tests':
    case 'webkit_unit_tests':
        switch (groupName) {
        case '@ToT - chromium.org':
            builderFilter = isChromiumWebkitTipOfTreeTestRunner;
            break;
        case '@DEPS - chromium.org':
            builderFilter = isChromiumWebkitDepsTestRunner;
            break;
        }
        break;
    }

    return builderFilter;
}

function populateBuilderToMaster()
{
    var allMasterNames = Object.keys(builders.masters);

    allMasterNames.forEach(function(masterName) {
        var master = builders.masters[masterName];
        var testTypes = Object.keys(master.tests);
        testTypes.forEach(function (testType) {
            var builderList = master.tests[testType].builders;
            builderList.forEach(function (builderName) {
                BUILDER_TO_MASTER[builderName] = builders.masters[masterName];
            });
        });
    });
}

// FIXME: should this be metadata?
function isToTBlink(groupName)
{
    return groupName.indexOf('@ToT') >= 0;
}

function groupNamesForTestType(testType)
{
    var groupNames = [];
    for (masterName in builders.masters) {
        var master = builders.masters[masterName];
        if (testType in master.tests) {
            groupNames = groupNames.concat(master.groups);
        }
    }

    if (groupNames.length == 0) {
        console.error("The current test type wasn't present in any groups:", testType);
        return groupNames;
    }

    var groupNames = groupNames.reduce(function(prev, curr) {
        if (prev.indexOf(curr) == -1) {
            prev.push(curr);
        }
        return prev;
    }, []);

    return groupNames;
}

