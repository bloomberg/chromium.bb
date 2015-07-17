function checkStateTransition(options) {
    debug("Check state transition for " + options.method + " on " +
          options.initialconnection + " state.");
    debug("- check initial state.");
    window.port = options.port;
    shouldBeEqualToString("port.connection", options.initialconnection);
    var checkHandler = function(e) {
        window.eventport = e.port;
        testPassed("handler is called with port " + eventport + ".");
        if (options.initialconnection == options.finalconnection) {
            testFailed("onstatechange handler should not be called here.");
        }
        shouldBeEqualToString("eventport.id", options.port.id);
        shouldBeEqualToString("eventport.connection", options.finalconnection);
    };
    port.onstatechange = function(e) {
        debug("- check port handler.");
        checkHandler(e);
    };
    access.onstatechange = function(e) {
        debug("- check access handler.");
        checkHandler(e);
    };
    if (options.method == "send") {
        port.send([]);
    }
    if (options.method == "setonmidimessage") {
        port.onmidimessage = function() {};
    }
    if (options.method == "addeventlistener") {
        port.addEventListener("midimessage", function() {});
    }
    if (options.method == "send" || options.method == "setonmidimessage" ||
        options.method == "addeventlistener") {
        // Following tests expect an implicit open finishes synchronously.
        // But it will be asynchronous in the future.
        debug("- check final state.");
        shouldBeEqualToString("port.connection", options.finalconnection);
        return Promise.resolve();
    }
    // |method| is expected to be "open" or "close".
    return port[options.method]().then(function(p) {
        window.callbackport = p;
        debug("- check callback arguments.");
        testPassed("callback is called with port " + callbackport + ".");
        shouldBeEqualToString("callbackport.id", options.port.id);
        shouldBeEqualToString("callbackport.connection", options.finalconnection);
        debug("- check final state.");
        shouldBeEqualToString("port.connection", options.finalconnection);
    }, function(e) {
        testFailed("error callback should not be called here.");
        throw e;
    });
}


