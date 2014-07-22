/* This is the helper function to run animation tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    callbacks [optional]: a function to be executed immediately after animation starts;
                          or, an object in the form {time: function} containing functions to be
                          called at the specified times (in seconds) during animation.
    trigger [optional]: a function to trigger transitions at the start of the test

    Each sub-array must contain these items in this order:
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

// Set to true to log debug information in failing tests. Note that these logs
// contain timestamps, so are non-deterministic and will introduce flakiness if
// any expected results include failures.
var ENABLE_ERROR_LOGGING = false;

function isCloseEnough(actual, desired, tolerance)
{
    if (typeof desired === "string")
        return actual === desired;
    var diff = Math.abs(actual - desired);
    return diff <= tolerance;
}

function roundNumber(num, decimalPlaces)
{
  return Math.round(num * Math.pow(10, decimalPlaces)) / Math.pow(10, decimalPlaces);
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
    var functionParse = s.match(/(\w+)\((.+)\)/);
    if (!functionParse)
        return null;

    var name = functionParse[1];
    var params = functionParse[2];
    params = params.split(/\s*[,\s]\s*/);

    // Parse numbers and normalize percentages
    for (var i = 0; i < params.length; ++i) {
        var param = params[i];
        if (!/$\d/.test(param))
            continue;
        params[i] = parseFloat(params[i]);
        if (param.indexOf('%') != -1)
            params[i] = params[i] / 100;
    }

    return {"shape": name, "params": params};
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

function filterParametersMatch(paramList1, paramList2, tolerance)
{
    if (paramList1.length != paramList2.length)
        return false;
    for (var i = 0; i < paramList1.length; ++i) {
        var param1 = paramList1[i], 
            param2 = paramList2[i];
        var match = isCloseEnough(param1, param2, tolerance);
        if (!match)
            return false;
    }
    return true;
}

function checkExpectedValue(expected, index)
{
    log('Checking expectation: ' + JSON.stringify(expected[index]));
    var time = expected[index][0];
    var elementId = expected[index][1];
    var property = expected[index][2];
    var expectedValue = expected[index][3];
    var tolerance = expected[index][4];

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

function compareRGB(rgb, expected, tolerance)
{
    return (isCloseEnough(parseInt(rgb[0]), expected[0], tolerance) &&
            isCloseEnough(parseInt(rgb[1]), expected[1], tolerance) &&
            isCloseEnough(parseInt(rgb[2]), expected[2], tolerance));
}

function checkExpectedTransitionValue(expected, index)
{
    log('Checking expectation: ' + JSON.stringify(expected[index]));
    var time = expected[index][0];
    var elementId = expected[index][1];
    var property = expected[index][2];
    var expectedValue = expected[index][3];
    var tolerance = expected[index][4];
    var postCompletionCallback = expected[index][5];

    var computedValue;
    var pass = false;
    var transformRegExp = /^-webkit-transform(\.\d+)?$/;
    if (transformRegExp.test(property)) {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).webkitTransform;
        if (typeof expectedValue === "string" || computedValue === "none")
            pass = (computedValue == expectedValue);
        else if (typeof expectedValue === "number") {
            var m = computedValue.split("(");
            var m = m[1].split(",");
            pass = isCloseEnough(parseFloat(m[parseInt(property.substring(18))]), expectedValue, tolerance);
        } else {
            var m = computedValue.split("(");
            var m = m[1].split(",");
            for (i = 0; i < expectedValue.length; ++i) {
                pass = isCloseEnough(parseFloat(m[i]), expectedValue[i], tolerance);
                if (!pass)
                    break;
            }
        }
    } else if (property == "fill" || property == "stroke" || property == "stop-color" || property == "flood-color" || property == "lighting-color") {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
        // The computedValue cssText is rgb(num, num, num)
        var components = computedValue.cssText.split("(")[1].split(")")[0].split(",");
        if (compareRGB(components, expectedValue, tolerance))
            pass = true;
        else {
            // We failed. Make sure computed value is something we can read in the error message
            computedValue = computedValue.cssText;
        }
    } else if (property == "lineHeight") {
        computedValue = parseInt(window.getComputedStyle(document.getElementById(elementId)).lineHeight);
        pass = isCloseEnough(computedValue, expectedValue, tolerance);
    } else if (property == "background-image"
               || property == "border-image-source"
               || property == "border-image"
               || property == "list-style-image"
               || property == "-webkit-mask-image"
               || property == "-webkit-mask-box-image") {
        if (property == "border-image" || property == "-webkit-mask-image" || property == "-webkit-mask-box-image")
            property += "-source";

        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property).cssText;
        computedCrossFade = parseCrossFade(computedValue);

        if (!computedCrossFade) {
            pass = false;
        } else {
            pass = isCloseEnough(computedCrossFade.percent, expectedValue, tolerance);
        }
    } else if (property == "object-position") {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).objectPosition;
        var actualArray = computedValue.split(" ");
        var expectedArray = expectedValue.split(" ");
        if (actualArray.length != expectedArray.length) {
            pass = false;
        } else {
            for (i = 0; i < expectedArray.length; ++i) {
                pass = isCloseEnough(parseFloat(actualArray[i]), parseFloat(expectedArray[i]), tolerance);
                if (!pass)
                    break;
            }
        }
    } else if (property === "shape-outside") {
        computedValue = window.getComputedStyle(document.getElementById(elementId)).getPropertyValue(property);
        var actualShape = parseBasicShape(computedValue);
        var expectedShape = parseBasicShape(expectedValue);
        pass = basicShapeParametersMatch(actualShape, expectedShape, tolerance);
    } else {
        var computedStyle = window.getComputedStyle(document.getElementById(elementId)).getPropertyCSSValue(property);
        if (computedStyle.cssValueType == CSSValue.CSS_VALUE_LIST) {
            var values = [];
            for (var i = 0; i < computedStyle.length; ++i) {
                switch (computedStyle[i].cssValueType) {
                  case CSSValue.CSS_PRIMITIVE_VALUE:
                    if (computedStyle[i].primitiveType == CSSPrimitiveValue.CSS_STRING)
                        values.push(computedStyle[i].getStringValue());
                    else
                        values.push(computedStyle[i].getFloatValue(CSSPrimitiveValue.CSS_NUMBER));
                    break;
                  case CSSValue.CSS_CUSTOM:
                    // arbitrarily pick shadow-x and shadow-y
                    if (property == 'box-shadow' || property == 'text-shadow') {
                      var text = computedStyle[i].cssText;
                      // Shadow cssText looks like "rgb(0, 0, 255) 0px -3px 10px 0px" and can be fractional.
                      var shadowPositionRegExp = /\)\s*(-?[\d.]+)px\s*(-?[\d.]+)px/;
                      var match = shadowPositionRegExp.exec(text);
                      var shadowXY = [parseInt(match[1]), parseInt(match[2])];
                      values.push(shadowXY[0]);
                      values.push(shadowXY[1]);
                    } else
                      values.push(computedStyle[i].cssText);
                    break;
                }
            }
            computedValue = values.join(',');
            pass = true;
            for (var i = 0; i < values.length; ++i)
                pass &= isCloseEnough(values[i], expectedValue[i], tolerance);
        } else if (computedStyle.cssValueType == CSSValue.CSS_PRIMITIVE_VALUE) {
            switch (computedStyle.primitiveType) {
                case CSSPrimitiveValue.CSS_STRING:
                case CSSPrimitiveValue.CSS_IDENT:
                    computedValue = computedStyle.getStringValue();
                    pass = computedValue == expectedValue;
                    break;
                case CSSPrimitiveValue.CSS_RGBCOLOR:
                    var rgbColor = computedStyle.getRGBColorValue();
                    computedValue = [rgbColor.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     rgbColor.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     rgbColor.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)]; // alpha is not exposed to JS
                    pass = true;
                    for (var i = 0; i < 3; ++i)
                        pass &= isCloseEnough(computedValue[i], expectedValue[i], tolerance);
                    break;
                case CSSPrimitiveValue.CSS_RECT:
                    computedValue = computedStyle.getRectValue();
                    computedValue = [computedValue.top.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.right.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.bottom.getFloatValue(CSSPrimitiveValue.CSS_NUMBER),
                                     computedValue.left.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)];
                     pass = true;
                     for (var i = 0; i < 4; ++i)
                         pass &= isCloseEnough(computedValue[i], expectedValue[i], tolerance);
                    break;
                case CSSPrimitiveValue.CSS_PERCENTAGE:
                    computedValue = parseFloat(computedStyle.cssText);
                    pass = isCloseEnough(computedValue, expectedValue, tolerance);
                    break;
                default:
                    computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
                    pass = isCloseEnough(computedValue, expectedValue, tolerance);
            }
        }
    }

    if (pass)
        result += "PASS - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s saw something close to: " + expectedValue + "<br>";
    else
        result += "FAIL - \"" + property + "\" property for \"" + elementId + "\" element at " + time + "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";

    if (postCompletionCallback)
      result += postCompletionCallback();
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
               || !property.indexOf("webkitTransform")) {
        computedValue = window.getComputedStyle(element)[property.split(".")[0]];
    } else {
        var computedStyle = window.getComputedStyle(element).getPropertyCSSValue(property);
        try {
            computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
        } catch (e) {
            computedValue = computedStyle.cssText;
        }
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
    } else if (property == "webkitClipPath") {
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
        if (typeof expectedValue == "string")
            result = (computedValue == expectedValue);
        else
            result = isCloseEnough(computedValue, expectedValue, tolerance);
    }
    return result;
}

function endTest()
{
    log('Ending test');
    var resultElement = useResultElement ? document.getElementById('result') : document.documentElement;
    if (ENABLE_ERROR_LOGGING && result.indexOf('FAIL') >= 0)
        result += '<br>Log:<br>' + logMessages.join('<br>');
    resultElement.innerHTML = result;

    if (window.testRunner)
        testRunner.notifyDone();
}

function runChecksWithRAF(checks)
{
    var finished = true;
    var time = performance.now() - animStartTime;

    log('RAF callback, animation time: ' + time);
    for (var k in checks) {
        var checkTime = Number(k);
        if (checkTime > time) {
            finished = false;
            break;
        }
        log('Running checks for time: ' + checkTime + ', delay: ' + (time - checkTime));
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
        log('Pausing at time: ' + timeMs + ', current players: ' + document.timeline.getAnimationPlayers().length);
        internals.pauseAnimations(timeMs / 1000);
        checks[k].forEach(function(check) { check(); });
    }
    endTest();
}

function startTest(checks)
{
    if (hasPauseAnimationAPI)
        runChecksWithPauseAPI(checks);
    else {
        result += 'Warning this test is running in real-time and may be flaky.<br>';
        runChecksWithRAF(checks);
    }
}

var logMessages = [];
var useResultElement = false;
var result = "";
var hasPauseAnimationAPI;
var animStartTime;
var isTransitionsTest = false;

function log(message)
{
    logMessages.push(performance.now() + ' - ' + message);
}

function waitForAnimationsToStart(callback)
{
    if (document.timeline.getAnimationPlayers().length > 0) {
        callback();
    } else {
        setTimeout(waitForAnimationsToStart.bind(this, callback), 0);
    }
}

// FIXME: disablePauseAnimationAPI and doPixelTest
function runAnimationTest(expected, callbacks, trigger, disablePauseAnimationAPI, doPixelTest, startTestImmediately)
{
    log('runAnimationTest');
    if (!expected)
        throw "Expected results are missing!";

    hasPauseAnimationAPI = 'internals' in window;
    if (disablePauseAnimationAPI)
        hasPauseAnimationAPI = false;

    var checks = {};

    if (typeof callbacks == 'function') {
        checks[0] = [callbacks];
    } else for (var time in callbacks) {
        timeMs = Math.round(time * 1000);
        checks[timeMs] = [callbacks[time]];
    }

    for (var i = 0; i < expected.length; i++) {
        var expectation = expected[i];
        var timeMs = Math.round(expectation[0] * 1000);
        if (!checks[timeMs])
            checks[timeMs] = [];
        if (isTransitionsTest)
            checks[timeMs].push(checkExpectedTransitionValue.bind(null, expected, i));
        else
            checks[timeMs].push(checkExpectedValue.bind(null, expected, i));
    }

    var doPixelTest = Boolean(doPixelTest);
    useResultElement = doPixelTest;

    if (window.testRunner) {
        if (doPixelTest) {
            testRunner.dumpAsTextWithPixelResults();
        } else {
            testRunner.dumpAsText();
        }
        testRunner.waitUntilDone();
    }

    var started = false;

    function begin() {
        if (!started) {
            log('First ' + event + ' event fired');
            started = true;
            if (trigger) {
                trigger();
                document.documentElement.offsetTop
            }
            waitForAnimationsToStart(function() {
                log('Finished waiting for animations to start');
                animStartTime = performance.now();
                startTest(checks);
            });
        }
    }

    var startTestImmediately = Boolean(startTestImmediately);
    if (startTestImmediately) {
        begin();
    } else {
        var target = isTransitionsTest ? window : document;
        var event = isTransitionsTest ? 'load' : 'webkitAnimationStart';
        target.addEventListener(event, begin, false);
    }
}

/* This is the helper function to run transition tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

Function parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    trigger [optional]: a function to be executed just before the test starts (none by default)
    callbacks [optional]: an object in the form {timeS: function} specifing callbacks to be made during the test
    doPixelTest [optional]: whether to dump pixels during the test (false by default)
    disablePauseAnimationAPI [optional]: whether to disable the pause API and run a RAF-based test (false by default)

    Each sub-array must contain these items in this order:
    - the time in seconds at which to snapshot the CSS property
    - the id of the element on which to get the CSS property value
    - the name of the CSS property to get [1]
    - the expected value for the CSS property
    - the tolerance to use when comparing the effective CSS property value with its expected value

    [1] If the CSS property name is "-webkit-transform", expected value must be an array of 1 or more numbers corresponding to the matrix elements,
    or a string which will be compared directly (useful if the expected value is "none")
    If the CSS property name is "-webkit-transform.N", expected value must be a number corresponding to the Nth element of the matrix

*/
function runTransitionTest(expected, trigger, callbacks, doPixelTest, disablePauseAnimationAPI) {
    isTransitionsTest = true;
    runAnimationTest(expected, callbacks, trigger, disablePauseAnimationAPI, doPixelTest);
}
