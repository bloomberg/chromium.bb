function stripURL(url) {
    return url ? url.match( /[^\/]+\/?$/ )[0] : url;
}

function throwException(message) {
    throw new Error(message ? message : "An exception");
}

var errorsSeen = 0;
function dumpOnErrorArgumentValuesAndReturn(returnValue, callback) {
    window.onerror = function (message, url, line, column) {
        debug("window.onerror: \"" + message + "\" at " + stripURL(url) + " (Line: " + line + ", Column: " + column + ")");

        if (callback)
            callback(++errorsSeen);
        if (returnValue)
            debug("Returning 'true': the error should not be reported in the console as an unhandled exception.");
        else
            debug("Returning 'false': the error should be reported in the console as an unhandled exception.");
        return returnValue;
    };
}

function dumpErrorEventAndAllowDefault(callback) {
    window.addEventListener('error', function (e) {
        dumpErrorEvent(e)
        debug("Not calling e.preventDefault(): the error should be reported in the console as an unhandled exception.");
        if (callback)
            callback(++errorsSeen);
    });
}

function dumpErrorEventAndPreventDefault(callback) {
    window.addEventListener('error', function (e) {
        dumpErrorEvent(e);
        debug("Calling e.preventDefault(): the error should not be reported in the console as an unhandled exception.");
        e.preventDefault();
        if (callback)
            callback(++errorsSeen);
    });
}

var eventPassedToTheErrorListener = null;
var eventCurrentTarget = null;
function dumpErrorEvent(e) {
    debug("Handling '" + e.type + "' event (phase " + e.eventPhase + "): \"" + e.message + "\" at " + stripURL(e.filename) + ":" + e.lineno);

    eventPassedToTheErrorListener = e;
    eventCurrentTarget = e.currentTarget;
    shouldBe('eventPassedToTheErrorListener', 'window.event');
    shouldBe('eventCurrentTarget', 'window');
    eventPassedToTheErrorListener = null;
    eventCurrentTarget = null;
}
