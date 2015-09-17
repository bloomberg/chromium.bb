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

    // Sanitizes the given text for display in test results.
    function sanitize(text) {
        if (!text) {
            return "";
        }
        // Escape null characters, otherwise diff will think the file is binary.
        text = text.replace(/\0/g, "\\0");
        // Escape carriage returns as they break rietveld's difftools.
        return text.replace(/\r/g, "\\r");
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

    var didDispatchLoadEvent = false;
    var handleLoad = function() {
        didDispatchLoadEvent = true;
        window.removeEventListener('load', handleLoad);
    };
    window.addEventListener('load', handleLoad, false);

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
                var logDiv = document.getElementById('log');
                if ((isCSSWGTest() || isJSTest()) && logDiv) {
                    // Assume it's a CSSWG style test, and anything other than
                    // the log div isn't material to the testrunner output, so
                    // should be hidden from the text dump.
                    document.body.textContent = '';
                }
            }

            // Add results element to document.
            if (!document.body)
                document.documentElement.appendChild(document.createElement("body"));
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
