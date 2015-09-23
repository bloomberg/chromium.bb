// include resources/js-test.js before this file.

function getCounterValues(callback) {
    testRunner.resetTestHelperControllers();
    asyncGC(function() {
        var ret = {
          'numberOfLiveDocuments': window.internals.numberOfLiveDocuments(),
          'numberOfLiveAXObjects': window.internals.numberOfLiveAXObjects()
        };

        var refCountedInstances = JSON.parse(window.internals.dumpRefCountedInstanceCounts());
        for (typename in refCountedInstances)
            ret['numberOfInstances-'+typename] = refCountedInstances[typename];

        callback(ret);
    });

}

function compareValues(countersBefore, countersAfter, tolerance) {
    for (type in tolerance) {
        var before = countersBefore[type];
        var after = countersAfter[type];

        if (after - before <= tolerance[type])
            testPassed('The difference of counter "'+type+'" before and after the cycle is under the threshold of '+tolerance[type]+'.');
        else
            testFailed('counter "'+type+'" was '+before+' before and now '+after+' after the cycle. This exceeds the threshold of '+tolerance[type]+'.');
    }
}

function doLeakTest(src, tolerance) {
    var frame = document.createElement('iframe');
    document.body.appendChild(frame);
    function loadSourceIntoIframe(src, callback) {
        var originalSrc = frame.src;

        frame.onload = function() {
            if (frame.src === originalSrc)
                return true;

            callback();
            return true;
        };
        frame.src = src;
    }

    jsTestIsAsync = true;
    if (!window.internals) {
        debug("This test only runs on DumpRenderTree, as it requires existence of window.internals and cross-domain resource access check disabled.");
        finishJSTest();
    }

    loadSourceIntoIframe('about:blank', function() {
        // blank document loaded...
        getCounterValues(function(countersBefore) {
            loadSourceIntoIframe(src, function() {
                // target document loaded...

                loadSourceIntoIframe('about:blank', function() {
                    // target document unloaded...

                    // Measure counter values on next timer event. This is needed
                    // to correctly handle deref cycles for some ActiveDOMObjects
                    // such as XMLHttpRequest.
                    setTimeout(function() {
                        getCounterValues(function(countersAfter) {
                            compareValues(countersBefore, countersAfter, tolerance);
                            finishJSTest();
                        });
                    }, 0);
                });
            });
        });
    });
}

function htmlToUrl(html) {
    return 'data:text/html;charset=utf-8,' + html;
}

function grabScriptText(id) {
    return document.getElementById(id).innerText;
}

// include fast/js/resources/js-test-post.js after this file.
