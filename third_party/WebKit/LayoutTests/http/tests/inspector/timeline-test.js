function wrapCallFunctionForTimeline(f)
{
    var script = document.createElement("script");
    script.textContent = "(" + f.toString() + ")()\n//# sourceURL=wrapCallFunctionForTimeline.js";
    document.body.appendChild(script);
}

var initialize_Timeline = function() {

InspectorTest.preloadPanel("timeline");
Bindings.TempFile = InspectorTest.TempFileMock;

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
    timedOut: "formatAsTypeName"
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

InspectorTest.preloadPanel("timeline");
Bindings.TempFile = InspectorTest.TempFileMock;

InspectorTest.createTracingModel = function()
{
    return new SDK.TracingModel(new Bindings.TempFileBackingStorage("tracing"));
}

InspectorTest.tracingModel = function()
{
    return UI.panels.timeline._tracingModel;
}

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories, enableJSSampling)
{
    var categories = "-*,disabled-by-default-devtools.timeline*,devtools.timeline," + SDK.TracingModel.TopLevelEventCategory;
    if (additionalCategories)
        categories += "," + additionalCategories;
    var timelinePanel = UI.panels.timeline;
    var timelineController = InspectorTest.timelineController();
    timelinePanel._timelineController = timelineController;
    timelineController._startRecordingWithCategories(categories, enableJSSampling, tracingStarted);

    function tracingStarted()
    {
        InspectorTest.callFunctionInPageAsync(functionName).then(onPageActionsDone);
    }

    function onPageActionsDone()
    {
        InspectorTest.addSniffer(UI.panels.timeline, "loadingComplete", callback)
        timelineController.stopRecording();
    }
}

InspectorTest.timelineModel = function()
{
    return UI.panels.timeline._model;
}

InspectorTest.timelineFrameModel = function()
{
    return UI.panels.timeline._frameModel;
}

InspectorTest.setTraceEvents = function(timelineModel, tracingModel, events)
{
    tracingModel.reset();
    tracingModel.addEvents(events);
    tracingModel.tracingComplete();
    timelineModel.setEvents(tracingModel);
}

InspectorTest.createTimelineModelWithEvents = function(events)
{
    var tracingModel = new SDK.TracingModel(new Bindings.TempFileBackingStorage("tracing"));
    var timelineModel = new TimelineModel.TimelineModel(Timeline.TimelineUIUtils.visibleEventsFilter());
    InspectorTest.setTraceEvents(timelineModel, tracingModel, events);
    return timelineModel;
}

InspectorTest.timelineController = function()
{
    var mainTarget = SDK.targetManager.mainTarget();
    var timelinePanel =  UI.panels.timeline;
    return new Timeline.TimelineController(mainTarget, timelinePanel, timelinePanel._tracingModel);
}

InspectorTest.startTimeline = function(callback)
{
    var panel = UI.panels.timeline;
    InspectorTest.addSniffer(panel, "recordingStarted", callback);
    panel._toggleRecording();
};

InspectorTest.stopTimeline = function(callback)
{
    var panel = UI.panels.timeline;
    function didStop()
    {
        InspectorTest.deprecatedRunAfterPendingDispatches(callback);
    }
    InspectorTest.addSniffer(panel, "loadingComplete", didStop);
    panel._toggleRecording();
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

InspectorTest.loadTimelineRecords = function(records)
{
    var model = UI.panels.timeline._model;
    model.reset();
    records.forEach(model._addRecord, model);
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

InspectorTest.printTimelineRecords = function(typeName, formatter)
{
    InspectorTest.timelineModel().forAllRecords(InspectorTest._printTimlineRecord.bind(InspectorTest, typeName, formatter));
};

InspectorTest.detailsTextForTraceEvent = function(traceEvent)
{
    return Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(traceEvent,
        SDK.targetManager.mainTarget(),
        new Components.Linkifier());
}

InspectorTest.printTimelineRecordsWithDetails = function(typeName)
{
    function detailsFormatter(recordType, record)
    {
        if (recordType && recordType !== record.type())
            return;
        var event = record.traceEvent();
        InspectorTest.addResult("Text details for " + record.type() + ": " + InspectorTest.detailsTextForTraceEvent(event));
        if (TimelineModel.TimelineData.forEvent(event).warning)
            InspectorTest.addResult(record.type() + " has a warning");
    }

    InspectorTest.timelineModel().forAllRecords(InspectorTest._printTimlineRecord.bind(InspectorTest, typeName, detailsFormatter.bind(null, typeName)));
};

InspectorTest.walkTimelineEventTree = function(callback)
{
    var model = InspectorTest.timelineModel();
    var view = new Timeline.EventsTimelineTreeView(model, UI.panels.timeline._filters, null);
    var selection = Timeline.TimelineSelection.fromRange(model.minimumRecordTime(), model.maximumRecordTime());
    view.updateContents(selection);
    InspectorTest.walkTimelineEventTreeUnderNode(callback, view._currentTree, 0);
}

InspectorTest.walkTimelineEventTreeUnderNode = function(callback, root, level)
{
    var event = root.event;
    if (event)
        callback(event, level)
    var children = root.children ? root.children.values() : [];
    for (var child of children)
        InspectorTest.walkTimelineEventTreeUnderNode(callback, child, (level || 0) + 1);
}

InspectorTest.printTimestampRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelineRecords(InspectorTest.timelineModel().eventDividerRecords(), typeName, formatter);
};

InspectorTest.innerPrintTimelineRecords = function(records, typeName, formatter)
{
    for (var i = 0; i < records.length; ++i)
        InspectorTest._printTimlineRecord(typeName, formatter, records[i]);
};

InspectorTest._printTimlineRecord = function(typeName, formatter, record)
{
    if (typeName && record.type() === typeName)
        InspectorTest.printTimelineRecordProperties(record);
    if (formatter)
        formatter(record);
};

// Dump just the record name, indenting output on separate lines for subrecords
InspectorTest.dumpTimelineRecord = function(record, detailsCallback, level, filterTypes)
{
    if (typeof level !== "number")
        level = 0;
    var message = "";
    for (var i = 0; i < level ; ++i)
        message = "----" + message;
    if (level > 0)
        message = message + "> ";
    if (record.type() === TimelineModel.TimelineModel.RecordType.TimeStamp
        || record.type() === TimelineModel.TimelineModel.RecordType.ConsoleTime) {
        message += Timeline.TimelineUIUtils.eventTitle(record.traceEvent());
    } else  {
        message += record.type();
    }
    if (detailsCallback)
        message += " " + detailsCallback(record);
    InspectorTest.addResult(message);

    var children = record.children();
    var numChildren = children.length;
    for (var i = 0; i < numChildren; ++i) {
        if (filterTypes && filterTypes.indexOf(children[i].type()) == -1)
            continue;
        InspectorTest.dumpTimelineRecord(children[i], detailsCallback, level + 1, filterTypes);
    }
}

InspectorTest.dumpTimelineModelRecord = function(record, level)
{
    if (typeof level !== "number")
        level = 0;
    var prefix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    InspectorTest.addResult(prefix + record.type() + ": " + (Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(record.traceEvent(), null) || ""));

    var numChildren = record.children() ? record.children().length : 0;
    for (var i = 0; i < numChildren; ++i)
        InspectorTest.dumpTimelineModelRecord(record.children()[i], level + 1);
}

InspectorTest.dumpTimelineRecords = function(timelineRecords)
{
    for (var i = 0; i < timelineRecords.length; ++i)
        InspectorTest.dumpTimelineRecord(timelineRecords[i], 0);
};

InspectorTest.printTimelineRecordProperties = function(record)
{
    InspectorTest.printTraceEventProperties(record.traceEvent());
}

InspectorTest.printTraceEventPropertiesIfNameMatches = function(set, traceEvent)
{
    if (set.has(traceEvent.name))
        InspectorTest.printTraceEventProperties(traceEvent);
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

InspectorTest.findFirstTimelineRecord = function(type)
{
    return InspectorTest.findTimelineRecord(type, 0);
}

// Find the (n+1)th timeline record of a specific type.
InspectorTest.findTimelineRecord = function(type, n)
{
    var result;
    function findByType(record)
    {
        if (record.type() !== type)
            return false;
        if (n === 0) {
            result = record;
            return true;
        }
        n--;
        return false;
    }
    InspectorTest.timelineModel().forAllRecords(findByType);
    return result;
}

InspectorTest.FakeFileReader = function(input, delegate, callback)
{
    this._delegate = delegate;
    this._callback = callback;
    this._input = input;
    this._loadedSize = 0;
    this._fileSize = input.length;
};

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
    var record = InspectorTest.findTimelineRecord(recordType, index || 0);
    InspectorTest.addArray(TimelineModel.InvalidationTracker.invalidationEventsFor(record._event), InspectorTest.InvalidationFormatters, "", comment);
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
    const provider = UI.panels.timeline._flameChart._dataProvider;
    InspectorTest.addResult('Timeline Flame Chart');
    InspectorTest.dumpFlameChartProvider(provider, includeGroups);
}

InspectorTest.FakeFileReader.prototype = {
    start: function(output)
    {
        this._delegate.onTransferStarted(this);

        var length = this._input.length;
        var half = (length + 1) >> 1;

        var chunk = this._input.substring(0, half);
        this._loadedSize += chunk.length;
        output.write(chunk);
        this._delegate.onChunkTransferred(this);

        chunk = this._input.substring(half);
        this._loadedSize += chunk.length;
        output.write(chunk);
        this._delegate.onChunkTransferred(this);

        output.close();
        this._delegate.onTransferFinished(this);

        this._callback();
    },

    cancel: function() { },

    loadedSize: function()
    {
        return this._loadedSize;
    },

    fileSize: function()
    {
        return this._fileSize;
    },

    fileName: function()
    {
        return "fakeFile";
    }
};

InspectorTest.loadTimeline = function(timelineData)
{
    var timeline = UI.panels.timeline;

    function createFileReader(file, delegate)
    {
        return new InspectorTest.FakeFileReader(timelineData, delegate, timeline._saveToFile.bind(timeline));
    }

    InspectorTest.override(Timeline.TimelineLoader, "_createFileReader", createFileReader);
    timeline._loadFromFile({});
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
