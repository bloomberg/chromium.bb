var initialize_BindingsTest = function() {

InspectorTest.preloadModule("sources");

InspectorTest.dumpWorkspace = function() {
    var uiSourceCodes = Workspace.workspace.uiSourceCodes().slice();
    var urls = uiSourceCodes.map(code => code.url());
    urls = urls.map(url => {
        if (!url.startsWith('debugger://'))
            return url;
        return url.replace(/VM\d+/g, 'VM[XXX]');
    });

    urls.sort(String.caseInsensetiveComparator);
    InspectorTest.addResult(`Workspace: ${urls.length} uiSourceCodes.`);
    for (var url of urls) {
        InspectorTest.addResult('    ' + url);
    }
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

InspectorTest.markStep = function(title) {
    InspectorTest.addResult('\nRunning: ' + title);
}

}

