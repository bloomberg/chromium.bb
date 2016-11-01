/*
 * THIS FILE INTENTIONALLY LEFT BLANK
 *
 * More specifically, this file is intended for vendors to implement
 * code needed to integrate testharness.js tests with their own test systems.
 *
 * Typically such integration will attach callbacks when each test is
 * has run, using add_result_callback(callback(test)), or when the whole test file has
 * completed, using add_completion_callback(callback(tests, harness_status)).
 *
 * For more documentation about the callback functions and the
 * parameters they are called with see testharness.js
 */

(function() {

    // Setup for WebKit JavaScript tests
    if (self.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
        testRunner.setCanOpenWindows();
        testRunner.setCloseRemainingWindowsWhenComplete(true);
    }

    // Disable the default output of testharness.js.  The default output formats
    // test results into an HTML table.  When that table is dumped as text, no
    // spacing between cells is preserved, and it is therefore not readable. By
    // setting output to false, the HTML table will not be created.
    setup({"output":false});

    // Function used to convert the test status code into the corresponding
    // string
    function convertResult(resultStatus) {
        switch (resultStatus) {
        case 0:
            return "PASS";
        case 1:
            return "FAIL";
        case 2:
            return "TIMEOUT";
        default:
            return "NOTRUN";
        }
    }

    var localPathRegExp;
    if (document.URL.startsWith("file:///")) {
        var index = document.URL.indexOf("/imported/wpt");
        if (index >= 0) {
            var localPath = document.URL.substring("file:///".length, index + "/imported/wpt".length);
            localPathRegExp = new RegExp(localPath.replace(/(\W)/g, "\\$1"), "g");
        }
    }

    // Sanitizes the given text for display in test results.
    function sanitize(text) {
        if (!text) {
            return "";
        }
        // Escape null characters, otherwise diff will think the file is binary.
        text = text.replace(/\0/g, "\\0");
        // Escape carriage returns as they break rietveld's difftools.
        text = text.replace(/\r/g, "\\r");
        // Replace machine-dependent path with "...".
        if (localPathRegExp)
            text = text.replace(localPathRegExp, "...");
        return text;
    }

    // If the test has a meta tag named flags and the content contains "dom",
    // then it's a CSSWG test.
    function isCSSWGTest() {
        var flags = document.querySelector('meta[name=flags]'),
            content = flags ? flags.getAttribute('content') : null;
        return content && content.match(/\bdom\b/);
    }

    function isJSTest() {
        return !!document.querySelector('script[src*="/resources/testharness"]');
    }

    function isWPTManualTest() {
        var path = location.pathname;
        return /\/imported\/wpt\/.*-manual\.html$/.test(path);
    }

    function loadAutomationScript() {
        var testPath = location.pathname;
        var automationPath = testPath.replace(/\/imported\/wpt\/.*$/, '/imported/wpt_automation');

        // Export importAutomationScript for use by the automation scripts.
        window.importAutomationScript = function(relativePath) {
            var script = document.createElement('script');
            script.src = automationPath + relativePath;
            document.head.appendChild(script);
        }

        var src;
        if (testPath.includes('/imported/wpt/fullscreen/')) {
            // Fullscreen tests all use the same automation script.
            src = automationPath + '/fullscreen/auto-click.js';
        } else if (testPath.includes('/imported/wpt/pointerevents/')
                   || testPath.includes('/imported/wpt/uievents/')) {
            // Per-test automation scripts.
            src = testPath.replace(/\/imported\/wpt\/(.*)\.html$/, "/imported/wpt_automation/$1-automation.js");
        } else {
            return;
        }
        var script = document.createElement('script');
        script.src = src;
        document.head.appendChild(script);
    }

    var didDispatchLoadEvent = false;
    window.addEventListener('load', function() {
        didDispatchLoadEvent = true;
        if (isWPTManualTest()) {
            setTimeout(loadAutomationScript, 0);
        }
    }, { once: true });

    // Using a callback function, test results will be added to the page in a
    // manner that allows dumpAsText to produce readable test results.
    add_completion_callback(function (tests, harness_status) {

        // Create element to hold results.
        var results = document.createElement("pre");

        // Declare result string.
        var resultStr = "This is a testharness.js-based test.\n";

        // Check harness_status.  If it is not 0, tests did not execute
        // correctly, output the error code and message.
        if (harness_status.status != 0) {
            resultStr += "Harness Error. harness_status.status = " +
                harness_status.status +
                " , harness_status.message = " +
                harness_status.message +
                "\n";
        }
        // reflection tests contain huge number of tests, and Chromium code
        // review tool has the 1MB diff size limit. We merge PASS lines.
        if (document.URL.indexOf("/html/dom/reflection") >= 0) {
            for (var i = 0; i < tests.length; ++i) {
                if (tests[i].status == 0) {
                    var colon = tests[i].name.indexOf(':');
                    if (colon > 0) {
                        var prefix = tests[i].name.substring(0, colon + 1);
                        var j = i + 1;
                        for (; j < tests.length; ++j) {
                            if (!tests[j].name.startsWith(prefix) || tests[j].status != 0)
                                break;
                        }
                        if ((j - i) > 1) {
                            resultStr += convertResult(tests[i].status) +
                                " " + sanitize(prefix) + " " + (j - i) + " tests\n"
                            i = j - 1;
                            continue;
                        }
                    }
                }
                resultStr += convertResult(tests[i].status) + " " +
                    sanitize(tests[i].name) + " " +
                    sanitize(tests[i].message) + "\n";
            }
        } else {
            // Iterate through tests array and build string that contains
            // results for all tests.
            for (var i = 0; i < tests.length; ++i) {
                resultStr += convertResult(tests[i].status) + " " +
                    sanitize(tests[i].name) + " " +
                    sanitize(tests[i].message) + "\n";
            }
        }

        resultStr += "Harness: the test ran to completion.\n";

        // Set results element's textContent to the results string.
        results.textContent = resultStr;

        function done() {
            if (self.testRunner) {
                if (isCSSWGTest() || isJSTest()) {
                    // Anything isn't material to the testrunner output, so
                    // should be hidden from the text dump.
                    if (document.body && document.body.tagName == 'BODY')
                        document.body.textContent = '';
                }
            }

            // Add results element to document.
            if (!document.body || document.body.tagName != 'BODY') {
                if (!document.documentElement)
                    document.appendChild(document.createElement('html'));
                else if (document.body) // document.body is <frameset>.
                    document.body.remove();
                document.documentElement.appendChild(document.createElement("body"));
            }
            document.body.appendChild(results);

            if (self.testRunner)
                testRunner.notifyDone();
        }

        if (didDispatchLoadEvent || document.readyState != 'loading') {
            // This function might not be the last 'completion callback', and
            // another completion callback might generate more results.  So, we
            // don't dump the results immediately.
            setTimeout(done, 0);
        } else {
            // Parsing the test HTML isn't finished yet.
            window.addEventListener('load', done);
        }
    });

})();
