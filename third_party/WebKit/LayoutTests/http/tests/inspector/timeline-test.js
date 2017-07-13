function wrapCallFunctionForTimeline(f)
{
    var script = document.createElement("script");
    script.textContent = "(" + f.toString() + ")()\n//# sourceURL=wrapCallFunctionForTimeline.js";
    document.body.appendChild(script);
}

var initialize_Timeline = function() {

InspectorTest.preloadPanel("timeline");

// Scrub values when printing out these properties in the record or data field.
InspectorTest.timelinePropertyFormatters = {
    children: "formatAsTypeName",
    endTime: "formatAsTypeName",
    requestId: "formatAsTypeName",
    startTime: "formatAsTypeName",
    stackTrace: "formatAsTypeName",
    url: "formatAsURL",
    scriptName: "formatAsTypeName",
    scriptId: "formatAsTypeName",
    usedHeapSizeDelta: "skip",
    mimeType: "formatAsTypeName",
    id: "formatAsTypeName",
    timerId: "formatAsTypeName",
    scriptLine: "formatAsTypeName",
    layerId: "formatAsTypeName",
    lineNumber: "formatAsTypeName",
    columnNumber: "formatAsTypeName",
    frameId: "formatAsTypeName",
    frame: "formatAsTypeName",
    page: "formatAsTypeName",
    encodedDataLength: "formatAsTypeName",
    identifier: "formatAsTypeName",
    clip: "formatAsTypeName",
    root: "formatAsTypeName",
    backendNodeId: "formatAsTypeName",
    nodeId: "formatAsTypeName",
    rootNode: "formatAsTypeName",
    finishTime: "formatAsTypeName",
    thread: "formatAsTypeName",
    allottedMilliseconds: "formatAsTypeName",
    timedOut: "formatAsTypeName",
    networkTime: "formatAsTypeName",
};

InspectorTest.InvalidationFormatters = {
    _tracingEvent: "skip",
    cause: "formatAsInvalidationCause",
    frame: "skip",
    invalidatedSelectorId: "skip",
    invalidationList: "skip",
    invalidationSet: "skip",
    linkedRecalcStyleEvent: "skip",
    linkedLayoutEvent: "skip",
    nodeId: "skip",
    paintId: "skip",
    startTime: "skip",
};

InspectorTest.formatters.formatAsInvalidationCause = function(cause)
{
    if (!cause)
        return "<undefined>";
    var stackTrace;
    if (cause.stackTrace && cause.stackTrace.length)
        stackTrace = InspectorTest.formatters.formatAsURL(cause.stackTrace[0].url) + ":" + (cause.stackTrace[0].lineNumber + 1);
    return "{reason: " + cause.reason + ", stackTrace: " + stackTrace + "}";
}

InspectorTest.createTracingModel = function(events)
{
    var model = new SDK.TracingModel(new Bindings.TempFileBackingStorage("tracing"));
    model.addEvents(events);
    model.tracingComplete();
    return model;
}

InspectorTest.tracingModel = function()
{
    return UI.panels.timeline._performanceModel.tracingModel();
}

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories, enableJSSampling)
{
    var categories = "-*,disabled-by-default-devtools.timeline*,devtools.timeline," + SDK.TracingModel.TopLevelEventCategory;
    if (additionalCategories)
        categories += "," + additionalCategories;
    var timelinePanel = UI.panels.timeline;
    var timelineController = InspectorTest.timelineController();
    timelinePanel._timelineController = timelineController;
    timelineController._startRecordingWithCategories(categories, enableJSSampling).then(tracingStarted);

    function tracingStarted()
    {
        timelinePanel._recordingStarted();
        InspectorTest.callFunctionInPageAsync(functionName).then(onPageActionsDone);
    }

    function onPageActionsDone()
    {
        InspectorTest.runWhenTimelineIsReady(callback);
        timelineController.stopRecording();
    }
}

InspectorTest.performanceModel = function()
{
    return UI.panels.timeline._performanceModel;
}

InspectorTest.timelineModel = function()
{
    return InspectorTest.performanceModel().timelineModel();
}

InspectorTest.timelineFrameModel = function()
{
    return InspectorTest.performanceModel().frameModel();
}

InspectorTest.createPerformanceModelWithEvents = function(events)
{
    var tracingModel = new SDK.TracingModel(new Bindings.TempFileBackingStorage("tracing"));
    tracingModel.addEvents(events);
    tracingModel.tracingComplete();
    var performanceModel = new Timeline.PerformanceModel();
    performanceModel.setTracingModel(tracingModel);
    return performanceModel;
}

InspectorTest.timelineController = function()
{
    var performanceModel = new Timeline.PerformanceModel();
    UI.panels.timeline._pendingPerformanceModel = performanceModel;
    return new Timeline.TimelineController(InspectorTest.tracingManager, performanceModel, UI.panels.timeline);
}

InspectorTest.runWhenTimelineIsReady = function(callback)
{
    InspectorTest.addSniffer(UI.panels.timeline, "loadingComplete", () => callback());
}

InspectorTest.startTimeline = function(callback)
{
    var panel = UI.panels.timeline;
    InspectorTest.addSniffer(panel, "_recordingStarted", callback);
    panel._toggleRecording();
};

InspectorTest.stopTimeline = function(callback)
{
    InspectorTest.runWhenTimelineIsReady(callback);
    UI.panels.timeline._toggleRecording();
};

InspectorTest.evaluateWithTimeline = function(actions, doneCallback)
{
    InspectorTest.startTimeline(step1);
    function step1()
    {
        InspectorTest.evaluateInPage(actions, step2);
    }

    function step2()
    {
        InspectorTest.stopTimeline(doneCallback);
    }
}

InspectorTest.invokeAsyncWithTimeline = function(functionName, doneCallback)
{
    InspectorTest.startTimeline(step1);
    function step1()
    {
        InspectorTest.callFunctionInPageAsync(functionName).then(step2);
    }

    function step2()
    {
        InspectorTest.stopTimeline(InspectorTest.safeWrap(doneCallback));
    }
}

InspectorTest.performActionsAndPrint = function(actions, typeName, includeTimeStamps)
{
    function callback()
    {
        InspectorTest.printTimelineRecordsWithDetails(typeName);
        if (includeTimeStamps) {
            InspectorTest.addResult("Timestamp records: ");
            InspectorTest.printTimestampRecords(typeName);
        }
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateWithTimeline(actions, callback);
};

InspectorTest.printTimelineRecords = function(name)
{
    for (let event of InspectorTest.timelineModel().mainThreadEvents()) {
        if (event.name === name)
            InspectorTest.printTraceEventProperties(event);
    }
};

InspectorTest.printTimelineRecordsWithDetails = function(name)
{
    for (let event of InspectorTest.timelineModel().mainThreadEvents()) {
        if (name === event.name)
            InspectorTest.printTraceEventPropertiesWithDetails(event);
    }
};

InspectorTest.walkTimelineEventTree = function(callback)
{
    var performanceModel = InspectorTest.performanceModel();
    var view = new Timeline.EventsTimelineTreeView(UI.panels.timeline._filters, null);
    view.setModel(performanceModel);
    var selection = Timeline.TimelineSelection.fromRange(performanceModel.timelineModel().minimumRecordTime(), performanceModel.timelineModel().maximumRecordTime());
    view.updateContents(selection);
    InspectorTest.walkTimelineEventTreeUnderNode(callback, view._currentTree, 0);
}

InspectorTest.walkTimelineEventTreeUnderNode = function(callback, root, level)
{
    var event = root.event;
    if (event)
        callback(event, level)
    for (var child of root.children().values())
        InspectorTest.walkTimelineEventTreeUnderNode(callback, child, (level || 0) + 1);
}

InspectorTest.printTimestampRecords = function(typeName)
{
    var dividers = InspectorTest.timelineModel().eventDividers();
    for (var event of dividers) {
        if (event.name === typeName)
            InspectorTest.printTraceEventProperties(event);
    }
};

InspectorTest.forAllEvents = function(events, callback)
{
    let eventStack = [];
    for (let event of events) {
        while (eventStack.length && eventStack.peekLast().endTime <= event.startTime)
            eventStack.pop();
        callback(event, eventStack);
        if (event.endTime)
            eventStack.push(event);
    }
}

InspectorTest.printTraceEventProperties = function(traceEvent)
{
    InspectorTest.addResult(traceEvent.name + " Properties:");
    var data = traceEvent.args["beginData"] || traceEvent.args["data"];
    var frameId = data && data["frame"];
    var object = {
        data: traceEvent.args["data"] || traceEvent.args,
        endTime: traceEvent.endTime || traceEvent.startTime,
        frameId: frameId,
        stackTrace: TimelineModel.TimelineData.forEvent(traceEvent).stackTrace,
        startTime: traceEvent.startTime,
        type: traceEvent.name,
    };
    for (var field in object) {
        if (object[field] === null || object[field] === undefined)
            delete object[field];
    }
    InspectorTest.addObject(object, InspectorTest.timelinePropertyFormatters);
};

InspectorTest.printTraceEventPropertiesWithDetails = function(event)
{
    InspectorTest.printTraceEventProperties(event);
    const details = Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(event,
        SDK.targetManager.mainTarget(),
        new Components.Linkifier());
    InspectorTest.addResult(`Text details for ${event.name}: ${details}`);
    if (TimelineModel.TimelineData.forEvent(event).warning)
        InspectorTest.addResult(`${event.name} has a warning`);
}

InspectorTest.findTimelineEvent = function(name, index)
{
    return InspectorTest.timelineModel().mainThreadEvents().filter(e => e.name === name)[index || 0];
}

InspectorTest.findChildEvent = function(events, parentIndex, name)
{
    var endTime = events[parentIndex].endTime;
    for (var i = parentIndex + 1; i < events.length && (!events[i].endTime || events[i].endTime <= endTime); ++i) {
        if (events[i].name === name)
            return events[i];
    }
    return null;
}

InspectorTest.dumpFrame = function(frame)
{
    var fieldsToDump = ["cpuTime", "duration", "startTime", "endTime", "id", "mainThreadFrameId", "timeByCategory", "other", "scripting", "painting", "rendering", "committedFrom", "idle"];
    function formatFields(object)
    {
        var result = {};
        for (var key in object) {
            if (fieldsToDump.indexOf(key) < 0)
                continue;
            var value = object[key];
            if (typeof value === "number")
                value = Number(value.toFixed(7));
            else if (typeof value === "object" && value)
                value = formatFields(value);
            result[key] = value;
        }
        return result;
    }
    InspectorTest.addObject(formatFields(frame));
}

InspectorTest.dumpInvalidations = function(recordType, index, comment)
{
    var event = InspectorTest.findTimelineEvent(recordType, index || 0);
    InspectorTest.addArray(TimelineModel.InvalidationTracker.invalidationEventsFor(event), InspectorTest.InvalidationFormatters, "", comment);
}

InspectorTest.dumpFlameChartProvider = function(provider, includeGroups)
{
    var includeGroupsSet = includeGroups && new Set(includeGroups);
    var timelineData = provider.timelineData();
    var stackDepth = provider.maxStackDepth();
    var entriesByLevel = new Multimap();

    for (let i = 0; i < timelineData.entryLevels.length; ++i)
        entriesByLevel.set(timelineData.entryLevels[i], i);

    for (let groupIndex = 0; groupIndex < timelineData.groups.length; ++groupIndex) {
        const group = timelineData.groups[groupIndex];
        if (includeGroupsSet && !includeGroupsSet.has(group.name))
            continue;
        var maxLevel = groupIndex + 1 < timelineData.groups.length ? timelineData.groups[groupIndex + 1].firstLevel : stackDepth;
        InspectorTest.addResult(`Group: ${group.name}`);
        for (let level = group.startLevel; level < maxLevel; ++level) {
            InspectorTest.addResult(`Level ${level - group.startLevel}`);
            var entries = entriesByLevel.get(level);
            for (const index of entries) {
                const title = provider.entryTitle(index);
                const color = provider.entryColor(index);
                InspectorTest.addResult(`${title} (${color})`);
            }
        }
    }
}

InspectorTest.dumpTimelineFlameChart = function(includeGroups) {
    const provider = UI.panels.timeline._flameChart._mainDataProvider;
    InspectorTest.addResult('Timeline Flame Chart');
    InspectorTest.dumpFlameChartProvider(provider, includeGroups);
}

InspectorTest.loadTimeline = function(timelineData)
{
    var promise = new Promise(fulfill => InspectorTest.runWhenTimelineIsReady(fulfill));
    UI.panels.timeline._loadFromFile(new Blob([timelineData], {type: 'text/plain'}));
    return promise;
}

};

function generateFrames(count)
{
    var promise = Promise.resolve();
    for (let i = count; i > 0; --i)
        promise = promise.then(changeBackgroundAndWaitForFrame.bind(null, i));
    return promise;

    function changeBackgroundAndWaitForFrame(i)
    {
        document.body.style.backgroundColor = i & 1 ? "rgb(200, 200, 200)" : "rgb(240, 240, 240)";
        return waitForFrame();
    }
}

function waitForFrame()
{
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    if (window.testRunner)
        testRunner.capturePixelsAsyncThen(() => window.requestAnimationFrame(callback));
    else
        window.requestAnimationFrame(callback);
    return promise;
}
