var initialize_BindingsTest = function() {

InspectorTest.preloadModule("sources");

InspectorTest.dumpWorkspace = function(previousSnapshot) {
    var uiSourceCodes = Workspace.workspace.uiSourceCodes().slice();
    var urls = uiSourceCodes.map(code => code.url());
    urls = urls.map(url => {
        if (!url.startsWith('debugger://'))
            return url;
        return url.replace(/VM\d+/g, 'VM[XXX]');
    });

    urls.sort(String.caseInsensetiveComparator);
    var isAdded = new Array(urls.length).fill(false);
    var removedLines = [];
    if (previousSnapshot) {
        var diff = Diff.Diff.lineDiff(previousSnapshot, urls);
        var removedEntries = diff.filter(entry => entry[0] === Diff.Diff.Operation.Delete).map(entry => entry[1]);
        removedLines = [].concat.apply([], removedEntries);
        var index = 0;
        for (var entry of diff) {
            if (entry[0] === Diff.Diff.Operation.Delete)
                continue;
            if (entry[0] === Diff.Diff.Operation.Equal) {
                index += entry[1].length;
                continue;
            }
            for (var line of entry[1])
                isAdded[index++] = true;
        }
        var addedEntries = diff.filter(entry => entry[0] === Diff.Diff.Operation.Insert).map(entry => entry[1]);
        addedLines = [].concat.apply([], addedEntries);
    }

    InspectorTest.addResult(`Removed: ${removedLines.length} uiSourceCodes`);
    for (var url of removedLines)
        InspectorTest.addResult('[-] ' + url);
    InspectorTest.addResult(`Workspace: ${urls.length} uiSourceCodes.`);
    for (var i = 0; i < urls.length; ++i) {
        var url = urls[i];
        var prefix = isAdded[i] ? '[+] ' : '    ';
        InspectorTest.addResult(prefix + url);
    }
    return urls;
}

InspectorTest.attachFrame = function(frameId, url, evalSourceURL) {
    var evalSource = `(${attachFrame.toString()})('${frameId}', '${url}')`;
    if (evalSourceURL)
        evalSource += '//# sourceURL=' + evalSourceURL;
    return InspectorTest.evaluateInPageAsync(evalSource);

    function attachFrame(frameId, url) {
        var frame = document.createElement('iframe');
        frame.src = url;
        frame.id = frameId;
        document.body.appendChild(frame);
        return new Promise(x => frame.onload = x);
    }
}

InspectorTest.detachFrame = function(frameId, evalSourceURL) {
    var evalSource = `(${detachFrame.toString()})('${frameId}')`;
    if (evalSourceURL)
        evalSource += '//# sourceURL=' + evalSourceURL;
    return InspectorTest.evaluateInPagePromise(evalSource);

    function detachFrame(frameId) {
        var frame = document.getElementById(frameId);
        frame.remove();
    }
}

InspectorTest.navigateFrame = function(frameId, navigateURL, evalSourceURL) {
    var evalSource = `(${navigateFrame.toString()})('${frameId}', '${navigateURL}')`;
    if (evalSourceURL)
        evalSource += '//# sourceURL=' + evalSourceURL;
    return InspectorTest.evaluateInPageAsync(evalSource);

    function navigateFrame(frameId, url) {
        var frame = document.getElementById(frameId);
        frame.src = url;
        return new Promise(x => frame.onload = x);
    }
}

InspectorTest.attachShadowDOM = function(id, templateSelector, evalSourceURL) {
    var evalSource = `(${createShadowDOM.toString()})('${id}', '${templateSelector}')`;
    if (evalSourceURL)
        evalSource += '//# sourceURL=' + evalSourceURL;
    return InspectorTest.evaluateInPagePromise(evalSource);

    function createShadowDOM(id, templateSelector) {
        var shadowHost = document.createElement('div');
        shadowHost.setAttribute('id', id);
        let shadowRoot = shadowHost.attachShadow({mode: 'open'});
        var t = document.querySelector(templateSelector);
        var instance = t.content.cloneNode(true);
        shadowRoot.appendChild(instance);
        document.body.appendChild(shadowHost);
    }
}

InspectorTest.detachShadowDOM = function(id, evalSourceURL) {
    var evalSource = `(${removeShadowDOM.toString()})('${id}')`;
    if (evalSourceURL)
        evalSource += '//# sourceURL=' + evalSourceURL;
    return InspectorTest.evaluateInPagePromise(evalSource);

    function removeShadowDOM(id) {
        document.querySelector('#' + id).remove();
    }
}

InspectorTest.waitForStyleSheetRemoved = function(urlSuffix) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    InspectorTest.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetRemoved, onStyleSheetRemoved);
    return promise;

    function onStyleSheetRemoved(event) {
        var styleSheetHeader = event.data;
        if (!styleSheetHeader.resourceURL().endsWith(urlSuffix))
            return;
        InspectorTest.cssModel.removeEventListener(SDK.CSSModel.Events.StyleSheetRemoved, onStyleSheetRemoved);
        fulfill();
    }
}

InspectorTest.addSniffer(Bindings.CompilerScriptMapping.prototype, "_sourceMapAttachedForTest", onSourceMap, true);
InspectorTest.addSniffer(Bindings.SASSSourceMapping.prototype, "_sourceMapAttachedForTest", onSourceMap, true);
var sourceMapCallbacks = new Map();
function onSourceMap(sourceMap) {
    for (var urlSuffix of sourceMapCallbacks.keys()) {
        if (sourceMap.url().endsWith(urlSuffix)) {
            var callback = sourceMapCallbacks.get(urlSuffix);
            callback.call(null);
            sourceMapCallbacks.delete(urlSuffix);
        }
    }
}

InspectorTest.waitForSourceMap = function(sourceMapURLSuffix) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    sourceMapCallbacks.set(sourceMapURLSuffix, fulfill);
    return promise;
}

var locationPool = new Bindings.LiveLocationPool();
var nameSymbol = Symbol('LiveLocationNameForTest');
var createdSymbol = Symbol('LiveLocationCreated');

InspectorTest.createDebuggerLiveLocation = function(name, urlSuffix, lineNumber, columnNumber) {
    var script = InspectorTest.debuggerModel.scripts().find(script => script.sourceURL.endsWith(urlSuffix));
    var rawLocation = InspectorTest.debuggerModel.createRawLocation(script, lineNumber || 0, columnNumber || 0);
    return Bindings.debuggerWorkspaceBinding.createLiveLocation(rawLocation, updateDelegate.bind(null, name), locationPool);
}

InspectorTest.createCSSLiveLocation = function(name, urlSuffix, lineNumber, columnNumber) {
    var header = InspectorTest.cssModel.styleSheetHeaders().find(header => header.resourceURL().endsWith(urlSuffix));
    var rawLocation = new SDK.CSSLocation(header, lineNumber || 0, columnNumber || 0);
    return Bindings.cssWorkspaceBinding.createLiveLocation(rawLocation, updateDelegate.bind(null, name), locationPool);
}

function updateDelegate(name, liveLocation) {
    liveLocation[nameSymbol] = name;
    var hint = liveLocation[createdSymbol] ? '[ UPDATE ]' : '[ CREATE ]';
    liveLocation[createdSymbol] = true;
    InspectorTest.dumpLocation(liveLocation, hint);
}

InspectorTest.dumpLocation = function(liveLocation, hint) {
    hint = hint || '[  GET   ]';
    var prefix = `${hint}  LiveLocation-${liveLocation[nameSymbol]}: `;
    var uiLocation = liveLocation.uiLocation();
    if (!uiLocation) {
        InspectorTest.addResult(prefix + 'null');
        return;
    }
    InspectorTest.addResult(prefix + uiLocation.uiSourceCode.url() + ':' + uiLocation.lineNumber + ':' + uiLocation.columnNumber);
}

}

