var initialize_Timeline = function() {

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
    encodedDataLength: "formatAsTypeName",
    identifier: "formatAsTypeName",
    clip: "formatAsTypeName",
    root: "formatAsTypeName",
    backendNodeId: "formatAsTypeName",
    networkTime: "formatAsTypeName",
    thread: "formatAsTypeName"
};

InspectorTest.timelinePresentationModel = function()
{
    return WebInspector.panels.timeline._currentViews[0]._presentationModel;
}

InspectorTest.timelineModel = function()
{
    return WebInspector.panels.timeline._model;
}

InspectorTest.startTimeline = function(callback)
{
    var panel = WebInspector.inspectorView.panel("timeline");
    function onRecordingStarted()
    {
        panel._model.removeEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onRecordingStarted, this)
        callback();
    }
    panel._model.addEventListener(WebInspector.TimelineModel.Events.RecordingStarted, onRecordingStarted, this)
    panel.toggleTimelineButton.element.click();
};

InspectorTest.stopTimeline = function(callback)
{
    var panel = WebInspector.inspectorView.panel("timeline");
    function didStop()
    {
        panel._model.removeEventListener(WebInspector.TimelineModel.Events.RecordingStopped, didStop, this)
        callback();
    }
    panel._model.addEventListener(WebInspector.TimelineModel.Events.RecordingStopped, didStop, this)
    panel.toggleTimelineButton.element.click();
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
       InspectorTest.stopTimeline(doneCallback);
    }
}

InspectorTest.loadTimelineRecords = function(records)
{
    var model = WebInspector.inspectorView.showPanel("timeline")._model;
    model.reset();
    records.forEach(model._addRecord, model);
}

InspectorTest.performActionsAndPrint = function(actions, typeName, includeTimeStamps)
{
    function callback()
    {
        InspectorTest.printTimelineRecords(typeName);
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

InspectorTest.printTimelinePresentationRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelinePresentationRecords(WebInspector.panels.timeline._model.records(), typeName, formatter);
};

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
    if (typeName && record.type() === WebInspector.TimelineModel.RecordType[typeName])
        InspectorTest.printTimelineRecordProperties(record);
    if (formatter)
        formatter(record);
};


InspectorTest.innerPrintTimelinePresentationRecords = function(records, typeName, formatter)
{
    for (var i = 0; i < records.length; ++i) {
        if (typeName && records[i].type() === WebInspector.TimelineModel.RecordType[typeName])
            InspectorTest.printTimelineRecordProperties(records[i]);
        if (formatter)
            formatter(records[i]);
        InspectorTest.innerPrintTimelinePresentationRecords(records[i].children(), typeName, formatter);
    }
};

// Dump just the record name, indenting output on separate lines for subrecords
InspectorTest.dumpTimelineRecord = function(record, detailsCallback, level, filterTypes)
{
    if (typeof level !== "number")
        level = 0;
    var prefix = "";
    var suffix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    if (record.type() === WebInspector.TimelineModel.RecordType.TimeStamp
        || record.type() === WebInspector.TimelineModel.RecordType.ConsoleTime) {
        suffix = " : " + record.data().message;
    }
    if (detailsCallback)
        suffix += " " + detailsCallback(record);
    InspectorTest.addResult(prefix + record.type() + suffix);

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
    InspectorTest.addResult(prefix + record.type());

    var numChildren = record.children() ? record.children().length : 0;
    for (var i = 0; i < numChildren; ++i)
        InspectorTest.dumpTimelineModelRecord(record.children()[i], level + 1);
}

// Dump just the record name, indenting output on separate lines for subrecords
InspectorTest.dumpPresentationRecord = function(presentationRecord, detailsCallback, level, filterTypes)
{
    var record = !presentationRecord.presentationParent() ? null : presentationRecord.record();
    if (typeof level !== "number")
        level = 0;
    var prefix = "";
    var suffix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    if (presentationRecord.coalesced()) {
        suffix = " x " + presentationRecord.presentationChildren().length;
    } else if (record && (record.type() === WebInspector.TimelineModel.RecordType.TimeStamp
        || record.type() === WebInspector.TimelineModel.RecordType.ConsoleTime)) {
        suffix = " : " + record.data().message;
    }
    if (detailsCallback)
        suffix += " " + detailsCallback(presentationRecord);
    var typeString = record ? record.type() : "Root";
    InspectorTest.addResult(prefix + typeString + suffix);

    var numChildren = presentationRecord.presentationChildren() ? presentationRecord.presentationChildren().length : 0;
    for (var i = 0; i < numChildren; ++i) {
        if (filterTypes && filterTypes.indexOf(presentationRecord.presentationChildren()[i].record().type()) == -1)
            continue;
        InspectorTest.dumpPresentationRecord(presentationRecord.presentationChildren()[i], detailsCallback, level + 1, filterTypes);
    }
}

InspectorTest.dumpTimelineRecords = function(timelineRecords)
{
    for (var i = 0; i < timelineRecords.length; ++i)
        InspectorTest.dumpTimelineRecord(timelineRecords[i], 0);
};

InspectorTest.printTimelineRecordProperties = function(record)
{
    InspectorTest.addResult(record.type() + " Properties:");
    // Use this recursive routine to print the properties
    if (record instanceof WebInspector.TimelineModel.RecordImpl)
        record = record._record;
    InspectorTest.addObject(record, InspectorTest.timelinePropertyFormatters);
};

InspectorTest.findFirstTimelineRecord = function(type)
{
    var result;
    function findByType(record)
    {
        if (record.type() !== type)
            return false;
        result = record;
        return true;
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
    var fieldsToDump = ["cpuTime", "duration", "startTime", "endTime", "id", "mainThreadFrameId", "timeByCategory", "other", "scripting", "painting", "rendering", "committedFrom"];
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

};
