/* This is the helper function to run animation tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    callbacks [optional]: a function to be executed immediately after animation starts;
                          or, an object in the form {time: function} containing functions to be
                          called at the specified times (in seconds) during animation.
    event [optional]: which DOM event to wait for before starting the test ("webkitAnimationStart" by default)

    Each sub-array must contain these items in this order:
    FIXME: Remove the name element as it is no longer required.
    - deprecated: the name of the CSS animation (may be null)
    - the time in seconds at which to snapshot the CSS property
    - the id of the element on which to get the CSS property value [1]
    - the name of the CSS property to get [2]
    - the expected value for the CSS property
    - the tolerance to use when comparing the effective CSS property value with its expected value

    [1] If a single string is passed, it is the id of the element to test. If an array with 2 elements is passed they
    are the ids of 2 elements, whose values are compared for equality. In this case the expected value is ignored
    but the tolerance is used in the comparison. If the second element is prefixed with "static:", no animation on that
    element is required, allowing comparison with an unanimated "expected value" element.

    If a string with a '.' is passed, this is an element in an iframe. The string before the dot is the iframe id
    and the string after the dot is the element name in that iframe.

    [2] If the CSS property name is "webkitTransform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "webkitTransform.N", expected value must be a number corresponding to the Nth element of the matrix

*/

function isCloseEnough(actual, desired, tolerance)
{
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function matrixStringToArray(s)
{
    if (s == "none")
        return [ 1, 0, 0, 1, 0, 0 ];
    var m = s.split("(");
    m = m[1].split(")");
    return m[0].split(",");
}

function parseCrossFade(s)
{
    var matches = s.match("-webkit-cross-fade\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\)");

    if (!matches)
        return null;

    return {"from": matches[1], "to": matches[2], "percent": parseFloat(matches[3])}
}

function parseBasicShape(s)
{
    var shapeFunction = s.match(/(\w+)\((.+)\)/);
    if (!shapeFunction)
        return null;

    var matches;
    switch (shapeFunction[1]) {
    case "rectangle":
        matches = s.match("rectangle\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\,\\s*(.*)\\)");
        break;
    case "circle":
        matches = s.match("circle\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\)");
        break;
    case "ellipse":
        matches = s.match("ellipse\\((.*)\\s*,\\s*(.*)\\s*,\\s*(.*)\\,\\s*(.*)\\)");
        break;
    case "polygon":
        matches = s.match("polygon\\(nonzero, (.*)\\s+(.*)\\s*,\\s*(.*)\\s+(.*)\\s*,\\s*(.*)\\s+(.*)\\s*,\\s*(.*)\\s+(.*)\\)");
        break;
    default:
        return null;
    }

    if (!matches)
        return null;

    matches.shift();

    // Normalize percentage values.
    for (var i = 0; i < matches.length; ++i) {
        var param = matches[i];
        matches[i] = parseFloat(matches[i]);
        if (param.indexOf('%') != -1)
            matches[i] = matches[i] / 100;
    }

    return {"shape": shapeFunction[1], "params": matches};
}

function basicShapeParametersMatch(paramList1, paramList2, tolerance)
{
    if (paramList1.shape != paramList2.shape
        || paramList1.params.length != paramList2.params.length)
        return false;
    for (var i = 0; i < paramList1.params.length; ++i) {
        var param1 = paramList1.params[i], 
            param2 = paramList2.params[i];
        var match = isCloseEnough(param1, param2, tolerance);
        if (!match)
            return false;
    }
    return true;
}

// Return an array of numeric filter params in 0-1.
function getFilterParameters(s)
{
    var filterResult = s.match(/(\w+)\((.+)\)/);
    if (!filterResult)
        throw new Error("There's no filter in \"" + s + "\"");
    var filterParams = filterResult[2];
    if (filterResult[1] == "custom") {
        if (!window.getCustomFilterParameters)
            throw new Error("getCustomFilterParameters not found. Did you include custom-filter-parser.js?");
        return getCustomFilterParameters(filterParams);
    }
    var paramList = filterParams.split(' '); // FIXME: the spec may allow comma separation at some point.
    
    // Normalize percentage values.
    for (var i = 0; i < paramList.length; ++i) {
        var param = paramList[i];
        paramList[i] = parseFloat(paramList[i]);
        if (param.indexOf('%') != -1)
            paramList[i] = paramList[i] / 100;
    }

    return paramList;
}

function customFilterParameterMatch(param1, param2, tolerance)
{
    if (param1.type != "parameter") {
        // Checking for shader uris and other keywords. They need to be exactly the same.
        return (param1.type == param2.type && param1.value == param2.value);
    }

    if (param1.name != param2.name || param1.value.length != param2.value.length)
        return false;

    for (var j = 0; j < param1.value.length; ++j) {
        var val1 = param1.value[j],
            val2 = param2.value[j];
        if (val1.type != val2.type)
            return false;
        switch (val1.type) {
        case "function":
            if (val1.name != val2.name)
                return false;
            if (val1.arguments.length != val2.arguments.length) {
                console.error("Arguments length mismatch: ", val1.arguments.length, "/", val2.arguments.length);
                return false;
            }
            for (var t = 0; t < val1.arguments.length; ++t) {
                if (val1.arguments[t].type != "number" || val2.arguments[t].type != "number")
                    return false;
                if (!isCloseEnough(val1.arguments[t].value, val2.arguments[t].value, tolerance))
                    return false;
            }
            break;
        case "number":
            if (!isCloseEnough(val1.value, val2.value, tolerance))
                return false;
            break;
        default:
            console.error("Unsupported parameter type ", val1.type);
            return false;
        }
    }

    return true;
}

function filterParametersMatch(paramList1, paramList2, tolerance)
{
    if (paramList1.length != paramList2.length)
        return false;
    for (var i = 0; i < paramList1.length; ++i) {
        var param1 = paramList1[i], 
            param2 = paramList2[i];
        if (typeof param1 == "object") {
            // This is a custom filter parameter.
            if (!customFilterParameterMatch(param1, param2, tolerance))
                return false;
            continue;
        }
        var match = isCloseEnough(param1, param2, tolerance);
        if (!match)
            return false;
    }
    return true;
}

function checkExpectedValue(expected, index)
{
    var animationName = expected[index][0];
    var time = expected[index][1];
    var elementId = expected[index][2];
    var property = expected[index][3];
    var expectedValue = expected[index][4];
    var tolerance = expected[index][5];

    // Check for a pair of element Ids
    var compareElements = false;
    var element2Static = false;
    var elementId2;
    if (typeof elementId != "string") {
        if (elementId.length != 2)
            return;
            
        elementId2 = elementId[1];
        elementId = elementId[0];

        if (elementId2.indexOf("static:") == 0) {
            elementId2 = elementId2.replace("static:", "");
            element2Static = true;
        }

        compareElements = true;
    }

    // Check for a dot separated string
    var iframeId;
    if (!compareElements) {
        var array = elementId.split('.');
        if (array.length == 2) {
            iframeId = array[0];
            elementId = array[1];
        }
    }

    var computedValue, computedValue2;
    if (compareElements) {
        computedValue = getPropertyValue(property, elementId, iframeId);
        computedValue2 = getPropertyValue(property, elementId2, iframeId);

        if (comparePropertyValue(property, computedValue, computedValue2, tolerance))
            result += "PASS - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + 
                            "\" elements at " + time + "s are close enough to each other" + "<br>";
        else
            result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" and \"" + elementId2 + 
                            "\" elements at " + time + "s saw: \"" + computedValue + "\" and \"" + computedValue2 + 
                                            "\" which are not close enough to each other" + "<br>";
    } else {
        var elementName;
        if (iframeId)
            elementName = iframeId + '.' + elementId;
        else
            elementName = elementId;

        computedValue = getPropertyValue(property, elementId, iframeId);

        if (comparePropertyValue(property, computedValue, expectedValue, tolerance))
            result += "PASS - \"" + property + "\" property for \"" + elementName + "\" element at " + time + 
                            "s saw something close to: " + expectedValue + "<br>";
        else
            result += "FAIL - \"" + property + "\" property for \"" + elementName + "\" element at " + time + 
                            "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";
    }
}


function getPropertyValue(property, elementId, iframeId)
{
    var computedValue;
    var element;
    if (iframeId)
        element = document.getElementById(iframeId).contentDocument.getElementById(elementId);
    else
        element = document.getElementById(elementId);

    if (property == "lineHeight")
        computedValue = parseInt(window.getComputedStyle(element).lineHeight);
    else if (property == "backgroundImage"
               || property == "borderImageSource"
               || property == "listStyleImage"
               || property == "webkitMaskImage"
               || property == "webkitMaskBoxImage"
               || property == "webkitFilter"
               || property == "webkitClipPath"
               || property == "webkitShapeInside"
               || !property.indexOf("webkitTransform")) {
        computedValue = window.getComputedStyle(element)[property.split(".")[0]];
    } else {
        var computedStyle = window.getComputedStyle(element).getPropertyCSSValue(property);
        computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
    }

    return computedValue;
}

function comparePropertyValue(property, computedValue, expectedValue, tolerance)
{
    var result = true;

    if (!property.indexOf("webkitTransform")) {
        if (typeof expectedValue == "string")
            result = (computedValue == expectedValue);
        else if (typeof expectedValue == "number") {
            var m = matrixStringToArray(computedValue);
            result = isCloseEnough(parseFloat(m[parseInt(property.substring(16))]), expectedValue, tolerance);
        } else {
            var m = matrixStringToArray(computedValue);
            for (i = 0; i < expectedValue.length; ++i) {
                result = isCloseEnough(parseFloat(m[i]), expectedValue[i], tolerance);
                if (!result)
                    break;
            }
        }
    } else if (property == "webkitFilter") {
        var filterParameters = getFilterParameters(computedValue);
        var filter2Parameters = getFilterParameters(expectedValue);
        result = filterParametersMatch(filterParameters, filter2Parameters, tolerance);
    } else if (property == "webkitClipPath" || property == "webkitShapeInside") {
        var clipPathParameters = parseBasicShape(computedValue);
        var clipPathParameters2 = parseBasicShape(expectedValue);
        if (!clipPathParameters || !clipPathParameters2)
            result = false;
        result = basicShapeParametersMatch(clipPathParameters, clipPathParameters2, tolerance);
    } else if (property == "backgroundImage"
               || property == "borderImageSource"
               || property == "listStyleImage"
               || property == "webkitMaskImage"
               || property == "webkitMaskBoxImage") {
        var computedCrossFade = parseCrossFade(computedValue);

        if (!computedCrossFade) {
            result = false;
        } else {
            if (typeof expectedValue == "string") {
                var computedCrossFade2 = parseCrossFade(expectedValue);
                result = isCloseEnough(computedCrossFade.percent, computedCrossFade2.percent, tolerance) && computedCrossFade.from == computedCrossFade2.from && computedCrossFade.to == computedCrossFade2.to;
            } else {
                result = isCloseEnough(computedCrossFade.percent, expectedValue, tolerance)
            }
        }
    } else {
        result = isCloseEnough(computedValue, expectedValue, tolerance);
    }
    return result;
}

function endTest()
{
    var resultElement = useResultElement ? document.getElementById('result') : document.documentElement;
    resultElement.innerHTML = result;

    if (window.testRunner)
        testRunner.notifyDone();
}

function runChecksWithRAF(checks)
{
    var finished = true;
    var time = performance.now() - animStartTime;

    for (var k in checks) {
        var checkTime = Number(k);
        if (checkTime > time) {
            finished = false;
            break;
        }
        checks[k].forEach(function(check) { check(); });
        delete checks[k];
    }

    if (finished)
        endTest();
    else
        requestAnimationFrame(runChecksWithRAF.bind(null, checks));
}

function runChecksWithPauseAPI(checks) {
    for (var k in checks) {
        var timeMs = Number(k);
        internals.pauseAnimations(timeMs / 1000);
        checks[k].forEach(function(check) { check(); });
    }
    endTest();
}

function startTest(checks)
{
    if (hasPauseAnimationAPI)
        runChecksWithPauseAPI(checks);
    else
        runChecksWithRAF(checks);
}

var useResultElement = false;
var result = "";
var hasPauseAnimationAPI;
var animStartTime;

// FIXME: remove deprecatedEvent, disablePauseAnimationAPI and doPixelTest
function runAnimationTest(expected, callbacks, deprecatedEvent, disablePauseAnimationAPI, doPixelTest)
{
    if (disablePauseAnimationAPI)
        result += 'Warning this test is running in real-time and may be flaky.<br>';
    if (deprecatedEvent)
        throw 'Event argument is deprecated!';
    if (!expected)
        throw "Expected results are missing!";

    hasPauseAnimationAPI = 'internals' in window;
    if (disablePauseAnimationAPI)
        hasPauseAnimationAPI = false;

    var checks = {};

    if (typeof callbacks == 'function')
        checks[0] = [callbacks];
    else for (var time in callbacks) {
        timeMs = Math.round(time * 1000);
        checks[timeMs] = [callbacks[time]];
    }

    for (var i = 0; i < expected.length; i++) {
        var expectation = expected[i];
        var timeMs = Math.round(expectation[1] * 1000);
        if (!checks[timeMs])
            checks[timeMs] = [];
        checks[timeMs].push(checkExpectedValue.bind(null, expected, i));
    }

    var doPixelTest = Boolean(doPixelTest);
    useResultElement = doPixelTest;

    if (window.testRunner) {
        testRunner.dumpAsText(doPixelTest);
        testRunner.waitUntilDone();
    }

    var started = false;
    document.addEventListener('webkitAnimationStart', function() {
        if (!started) {
            started = true;
            animStartTime = performance.now();
            // delay to give hardware animations a chance to start
            setTimeout(function() {
                startTest(checks);
            }, 0);
        }
    }, false);
}
