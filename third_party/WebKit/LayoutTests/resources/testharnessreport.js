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

// Setup for WebKit JavaScript tests.
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function convertResultStatusToString(resultStatus)
{
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

// Disable the default output of testharness.js.  The default output formats
// test results into an HTML table.  When that table is dumped as text, no
// spacing between cells is preserved, and it is therefore not readable. By
// setting output to false, the HTML table will not be created.
setup({output: false});

// Using a callback function, test results will be added to the page in a
// manner that allows dumpAsText to produce readable test results.
add_completion_callback(function (tests, harnessStatus)
{
    // An array to hold string pieces, which will be joined later to produce the final result.
    var resultsArray = ["\n"];

    if (harnessStatus.status !== 0) {
        resultsArray.push("Harness Error. harnessStatus.status = ",
                          harnessStatus.status,
                          " , harnessStatus.message = ",
                          harnessStatus.message);
    } else {
        for (var i = 0; i < tests.length; i++) {
            resultsArray.push(convertResultStatusToString(tests[i].status),
                              " ",
                              tests[i].name !== null ? tests[i].name : "",
                              " ",
                              tests[i].message !== null ? tests[i].message : "",
                              "\n");
        }
    }

    var resultElement = document.createElement("pre");
    resultElement.textContent = resultsArray.join("");
    document.body.appendChild(resultElement);

    if (window.testRunner)
        testRunner.notifyDone();
});
