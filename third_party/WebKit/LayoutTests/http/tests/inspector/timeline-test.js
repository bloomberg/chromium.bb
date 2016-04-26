function wrapCallFunctionForTimeline(f)
{
    var script = document.createElement("script");
    script.textContent = "(" + f.toString() + ")()\n//# sourceURL=wrapCallFunctionForTimeline.js";
    document.body.appendChild(script);
}

var initialize_Timeline = function() {

InspectorTest.preloadPanel("timeline");
WebInspector.TempFile = InspectorTest.TempFileMock;

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
    networkTime: "formatAsTypeName",
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
        stackTrace = InspectorTest.formatters.formatAsURL(cause.stackTrace[0].url) + ":" + cause.stackTrace[0].lineNumber;
    return "{reason: " + cause.reason + ", stackTrace: " + stackTrace + "}";
}

InspectorTest.preloadPanel("timeline");
WebInspector.TempFile = InspectorTest.TempFileMock;

InspectorTest.createTracingModel = function()
{
    return new WebInspector.TracingModel(new WebInspector.TempFileBackingStorage("tracing"));
}

InspectorTest.tracingModel = function()
{
    return WebInspector.panels.timeline._tracingModel;
}

InspectorTest.invokeWithTracing = function(functionName, callback, additionalCategories, enableJSSampling)
{
    var categories = "-*,disabled-by-default-devtools.timeline*,devtools.timeline," + WebInspector.TracingModel.TopLevelEventCategory;
    if (additionalCategories)
        categories += "," + additionalCategories;
    var timelinePanel = WebInspector.panels.timeline;
    var timelineController = InspectorTest.timelineController();
    timelinePanel._timelineController = timelineController;
    timelineController._startRecordingWithCategories(categories, enableJSSampling, tracingStarted);

    function tracingStarted()
    {
        InspectorTest.invokePageFunctionAsync(functionName, onPageActionsDone);
    }

    function onPageActionsDone()
    {
        InspectorTest.addSniffer(WebInspector.panels.timeline, "loadingComplete", callback)
        timelineController.stopRecording();
    }
}

InspectorTest.timelineModel = function()
{
    return WebInspector.panels.timeline._model;
}

InspectorTest.timelineFrameModel = function()
{
    return WebInspector.panels.timeline._frameModel;
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
    var tracingModel = new WebInspector.TracingModel(new WebInspector.TempFileBackingStorage("tracing"));
    var timelineModel = new WebInspector.TimelineModel(WebInspector.TimelineUIUtils.visibleEventsFilter());
    InspectorTest.setTraceEvents(timelineModel, tracingModel, events);
    return timelineModel;
}

InspectorTest.timelineController = function()
{
    var mainTarget = WebInspector.targetManager.mainTarget();
    var timelinePanel =  WebInspector.panels.timeline;
    return new WebInspector.TimelineController(mainTarget, timelinePanel, timelinePanel._tracingModel);
}

InspectorTest.startTimeline = function(callback)
{
    var panel = WebInspector.panels.timeline;
    InspectorTest.addSniffer(panel, "recordingStarted", callback);
    panel._toggleRecording();
};

InspectorTest.stopTimeline = function(callback)
{
    var panel = WebInspector.panels.timeline;
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
        InspectorTest.invokePageFunctionAsync(functionName, step2);
    }

    function step2()
    {
        InspectorTest.stopTimeline(InspectorTest.safeWrap(doneCallback));
    }
}

InspectorTest.loadTimelineRecords = function(records)
{
    var model = WebInspector.panels.timeline._model;
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
    return WebInspector.TimelineUIUtils.buildDetailsTextForTraceEvent(traceEvent,
        WebInspector.targetManager.mainTarget(),
        new WebInspector.Linkifier());
}

InspectorTest.printTimelineRecordsWithDetails = function(typeName)
{
    function detailsFormatter(recordType, record)
    {
        if (recordType && recordType !== record.type())
            return;
        var event = record.traceEvent();
        InspectorTest.addResult("Text details for " + record.type() + ": " + InspectorTest.detailsTextForTraceEvent(event));
        if (event.warning)
            InspectorTest.addResult(record.type() + " has a warning");
    }

    InspectorTest.timelineModel().forAllRecords(InspectorTest._printTimlineRecord.bind(InspectorTest, typeName, detailsFormatter.bind(null, typeName)));
};

InspectorTest.walkTimelineEventTree = function(callback)
{
    var model = InspectorTest.timelineModel();
    var view = new WebInspector.EventsTimelineTreeView(model, WebInspector.panels.timeline._filters, null);
    var selection = WebInspector.TimelineSelection.fromRange(model.minimumRecordTime(), model.maximumRecordTime());
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
    if (record.type() === WebInspector.TimelineModel.RecordType.TimeStamp
        || record.type() === WebInspector.TimelineModel.RecordType.ConsoleTime) {
        message += WebInspector.TimelineUIUtils.eventTitle(record.traceEvent());
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
    InspectorTest.addResult(prefix + record.type() + ": " + (WebInspector.TimelineUIUtils.buildDetailsTextForTraceEvent(record.traceEvent(), null) || ""));

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
        stackTrace: traceEvent.stackTrace,
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
    var timeline = WebInspector.panels.timeline;

    function createFileReader(file, delegate)
    {
        return new InspectorTest.FakeFileReader(timelineData, delegate, timeline._saveToFile.bind(timeline));
    }

    InspectorTest.override(WebInspector.TimelineLoader, "_createFileReader", createFileReader);
    timeline._loadFromFile({});
}

};

function generateFrames(count, callback)
{
    makeFrame();
    function makeFrame()
    {
        document.body.style.backgroundColor = count & 1 ? "rgb(200, 200, 200)" : "rgb(240, 240, 240)";
        if (!--count) {
            callback();
            return;
        }
        if (window.testRunner)
            testRunner.capturePixelsAsyncThen(requestAnimationFrame.bind(window, makeFrame));
        else
            window.requestAnimationFrame(makeFrame);
    }
}
