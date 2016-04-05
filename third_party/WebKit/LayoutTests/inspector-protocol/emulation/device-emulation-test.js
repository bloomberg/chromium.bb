function setMetaViewport(override)
{
    var VIEWPORTS = {
        "w=320": "width=320",
        "w=dw": "width=device-width",
        "w=980": "width=980",
        "none": "no viewport (desktop site)"
    };

    var viewport = VIEWPORTS["none"];
    for (var key in VIEWPORTS) {
        if (location.search.indexOf(key) !== -1) {
            viewport = VIEWPORTS[key];
            window.__viewport = key;
            break;
        }
    }
    if (override) {
        viewport = VIEWPORTS[override];
        window.__viewport = override;
    }
    if (viewport != VIEWPORTS["none"])
        document.write('<meta name="viewport" content="'+viewport+'">');
}

// matchMedia() polyfill from https://github.com/paulirish/matchMedia.js
// Authors & copyright (c) 2012: Scott Jehl, Paul Irish, Nicholas Zakas. Dual MIT/BSD license
window.matchMedia = window.matchMedia || (function(doc, undefined){
    var bool,
        docElem  = doc.documentElement,
        refNode  = docElem.firstElementChild || docElem.firstChild,
        // fakeBody required for <FF4 when executed in <head>
        fakeBody = doc.createElement('body'),
        div      = doc.createElement('div');

    div.id = 'mq-test-1';
    div.style.cssText = "position:absolute;top:-100em";
    fakeBody.style.background = "none";
    fakeBody.appendChild(div);

    return function(q){
        div.innerHTML = '&shy;<style media="'+q+'"> #mq-test-1 { width: 42px; }</style>';

        docElem.insertBefore(fakeBody, refNode);
        bool = div.offsetWidth == 42;
        docElem.removeChild(fakeBody);

        return { matches: bool, media: q };
    };
})(document);

var results;

function dumpMetrics(full)
{
    results = [];
    writeResult("Device:", "");
    testJS("window.screenX");
    testJS("window.screenY");

    writeResult("Viewport:", "?" + window.__viewport);

    testMQOrientation();
    testJS("window.orientation", "");

    if (full) {
        testMQDimension("resolution", null, "dpi");
        testMQDevicePixelRatio(window.devicePixelRatio);
        testJS("window.devicePixelRatio", "");
    }

    writeResult("Widths:", "");

    if (full) {
        testMQDimension("device-width", screen.width);
        testJS("screen.width");
        testJS("screen.availWidth");
        testJS("window.outerWidth");
        testJS("window.innerWidth");
        testMQDimension("width", document.documentElement.clientWidth);
    }
    testJS("document.documentElement.clientWidth");
    testJS("document.documentElement.offsetWidth");
    testJS("document.documentElement.scrollWidth");
    if (full)
        testJS("document.body.clientWidth");
    testJS("document.body.offsetWidth");
    testJS("document.body.scrollWidth");

    writeResult("Heights:", "");

    if (full) {
        testMQDimension("device-height", screen.height);
        testJS("screen.height");
        testJS("screen.availHeight");
        testJS("window.outerHeight");
        testJS("window.innerHeight");
        testMQDimension("height", document.documentElement.clientHeight);
    }
    testJS("document.documentElement.clientHeight");
    testJS("document.documentElement.offsetHeight");
    testJS("document.documentElement.scrollHeight");
    if (full)
        testJS("document.body.clientHeight");
    testJS("document.body.offsetHeight");
    testJS("document.body.scrollHeight");

    var measured = document.querySelectorAll(".device-emulation-measure");
    for (var i = 0; i < measured.length; ++i)
        writeResult("measured " + measured[i].getAttribute("type") + ": " + measured[i].offsetWidth + "x" + measured[i].offsetHeight);

    return results.join("\n");
}

function testJS(expr, unit)
{
    if (unit === undefined)
        unit = "px";

    var ans = eval(expr);
    if (typeof ans == "number")
        ans += unit;

    // Shorten long properties to make more readable
    expr = expr.replace("documentElement", "docElem").replace("document", "doc");

    writeResult(expr, ans);
}

function testMQOrientation()
{
    if (matchMedia("screen and (orientation: portrait)").matches)
        writeResult("@media orientation", "portrait");
    else if (matchMedia("screen and (orientation: landscape)").matches)
        writeResult("@media orientation", "landscape");
    else
        writeResult("@media orientation", "???");
}

function testMQDevicePixelRatio(guess)
{
    // To save time, try the guess value first; otherwise try common values (TODO: binary search).
    if (matchMedia("screen and (-webkit-device-pixel-ratio: "+guess+"), screen and (device-pixel-ratio: "+guess+")").matches)
        writeResult("@media device-pixel-ratio", guess);
    else if (matchMedia("screen and (-webkit-device-pixel-ratio: 2), screen and (device-pixel-ratio: 2)").matches)
        writeResult("@media device-pixel-ratio", "2");
    else if (matchMedia("screen and (-webkit-device-pixel-ratio: 1.5), screen and (device-pixel-ratio: 1.5)").matches)
        writeResult("@media device-pixel-ratio", "1.5");
    else if (matchMedia("screen and (-webkit-device-pixel-ratio: 1), screen and (device-pixel-ratio: 1)").matches)
        writeResult("@media device-pixel-ratio", "1");
    else if (matchMedia("screen and (-webkit-device-pixel-ratio: 2.25), screen and (device-pixel-ratio: 2.25)").matches)
        writeResult("@media device-pixel-ratio", "2.25");
    else
        writeResult("@media device-pixel-ratio", "???");
}

function testMQDimension(feature, guess, unit)
{
    unit = unit || "px";
    // To save time, try the guess value first; otherwise binary search.
    if (guess && matchMedia("screen and (" + feature + ":" + guess + unit + ")").matches) {
        writeResult("@media " + feature, guess + unit);
    } else {
        var val = mqBinarySearch(feature, 1, 2560, unit);
        writeResult("@media " + feature, val ? val + unit : "???");
    }
}

// Searches the inclusive range [minValue, maxValue].
function mqBinarySearch(feature, minValue, maxValue, unit)
{
    if (minValue == maxValue) {
        if (matchMedia("screen and (" + feature + ":" + minValue + unit + ")").matches)
            return minValue;
        else
            return null;
    }
    var mid = Math.ceil((minValue + maxValue) / 2);
    if (matchMedia("screen and (min-" + feature + ":" + mid + unit + ")").matches)
        return mqBinarySearch(feature, mid, maxValue, unit); // feature is >= mid
    else
        return mqBinarySearch(feature, minValue, mid - 1, unit); // feature is < mid
}

function writeResult(key, val)
{
    results.push(key + (val ? " = " + val : ""));
}

var initialize_DeviceEmulationTest = function() {

InspectorTest.getPageMetrics = function(full, callback)
{
    InspectorTest.evaluateInPage("dumpMetrics(" + full + ")", callback);
}

InspectorTest.applyEmulationAndReload = function(enabled, width, height, deviceScaleFactor, viewport, insets, noReload, callback)
{
    if (enabled) {
        var params = {};
        params.width = width;
        params.height = height;
        params.deviceScaleFactor = deviceScaleFactor;
        params.mobile = true;
        params.fitWindow = false;
        params.scale = 1;
        params.screenWidth = width;
        params.screenHeight = height;
        params.positionX = 0;
        params.positionY = 0;
        if (insets) {
            params.screenWidth += insets.left + insets.right;
            params.positionX = insets.left;
            params.screenHeight += insets.top + insets.bottom;
            params.positionY = insets.top;
        }
        InspectorTest.sendCommand("Emulation.setDeviceMetricsOverride", params, emulateCallback);
    } else {
        InspectorTest.sendCommand("Emulation.clearDeviceMetricsOverride", {}, emulateCallback);
    }

    function emulateCallback(result)
    {
        if (result.error)
            InspectorTest._deviceEmulationResults.push("Error: " + result.error);
        if (noReload)
            callback();
        else
            InspectorTest.navigate(InspectorTest._deviceEmulationPageUrl + "?" + viewport, callback);
    }
};

InspectorTest.emulateAndGetMetrics = function(width, height, deviceScaleFactor, viewport, insets, noReload, callback)
{
    InspectorTest._deviceEmulationResults.push("Emulating device: " + width + "x" + height + "x" + deviceScaleFactor + " viewport='" + viewport + "'");
    var full = !!width && !!height && !!deviceScaleFactor;
    InspectorTest.applyEmulationAndReload(true, width, height, deviceScaleFactor, viewport, insets, noReload, InspectorTest.getPageMetrics.bind(InspectorTest, full, printMetrics));

    function printMetrics(metrics)
    {
        InspectorTest._deviceEmulationResults.push(metrics + "\n");
        callback();
    }
};

InspectorTest.testDeviceEmulation = function(pageUrl, width, height, deviceScaleFactor, viewport, insets, noReload)
{
    InspectorTest._deviceEmulationPageUrl = pageUrl;
    InspectorTest._deviceEmulationResults = [];
    InspectorTest.emulateAndGetMetrics(width, height, deviceScaleFactor, viewport, insets, noReload, callback);

    function callback()
    {
        InspectorTest.log(InspectorTest._deviceEmulationResults.join("\n"));
        InspectorTest.completeTest();
    }
};

};
