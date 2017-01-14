var initialize_ProfilerTest = function() {

InspectorTest.preloadPanel("js_profiler");
Bindings.TempFile = InspectorTest.TempFileMock;

InspectorTest.startProfilerTest = function(callback)
{
    InspectorTest.addResult("Profiler was enabled.");
    InspectorTest.addSniffer(UI.panels.js_profiler, "_addProfileHeader", InspectorTest._profileHeaderAdded, true);
    InspectorTest.addSniffer(Profiler.ProfileView.prototype, "refresh", InspectorTest._profileViewRefresh, true);
    InspectorTest.safeWrap(callback)();
};

InspectorTest.completeProfilerTest = function()
{
    InspectorTest.addResult("");
    InspectorTest.addResult("Profiler was disabled.");
    InspectorTest.completeTest();
};

InspectorTest.runProfilerTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeProfilerTest();
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startProfilerTest(runner);
};

InspectorTest.showProfileWhenAdded = function(title)
{
    InspectorTest._showProfileWhenAdded = title;
};

InspectorTest._profileHeaderAdded = function(profile)
{
    if (InspectorTest._showProfileWhenAdded === profile.title)
        UI.panels.js_profiler.showProfile(profile);
};

InspectorTest.waitUntilProfileViewIsShown = function(title, callback)
{
    callback = InspectorTest.safeWrap(callback);

    var profilesPanel = UI.panels.js_profiler;
    if (profilesPanel.visibleView && profilesPanel.visibleView.profile && profilesPanel.visibleView._profileHeader.title === title)
        callback(profilesPanel.visibleView);
    else
        InspectorTest._waitUntilProfileViewIsShownCallback = { title: title, callback: callback };
}

InspectorTest._profileViewRefresh = function()
{
    // Called in the context of ProfileView.
    if (InspectorTest._waitUntilProfileViewIsShownCallback && InspectorTest._waitUntilProfileViewIsShownCallback.title === this._profileHeader.title) {
        var callback = InspectorTest._waitUntilProfileViewIsShownCallback;
        delete InspectorTest._waitUntilProfileViewIsShownCallback;
        callback.callback(this);
    }
};

InspectorTest.startSamplingHeapProfiler = function()
{
    Profiler.SamplingHeapProfileType.instance.startRecordingProfile();
}

InspectorTest.stopSamplingHeapProfiler = function()
{
    Profiler.SamplingHeapProfileType.instance.stopRecordingProfile();
}

};
