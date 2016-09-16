// Test that constructor for the node with name |nodeName| handles the various
// possible values for channelCount, channelCountMode, and
// channelInterpretation.
function testAudioNodeOptions(context, nodeName, nodeOptions) {
    if (nodeOptions === undefined)
        nodeOptions = {};
    var node;
    var success = true;

    // Test that we can set channelCount and that errors are thrown for invalid values
    var testCount = 17;
    if (nodeOptions.expectedChannelCount) {
        testCount = nodeOptions.expectedChannelCount.value;
    }
    success = Should("new " + nodeName + "(c, {channelCount: " + testCount + "}}",
        function () {
            node = new window[nodeName](
                context,
                Object.assign({},
                    nodeOptions.additionalOptions, {
                        channelCount: testCount
                    }));
        }).notThrow();
    success = Should("node.channelCount", node.channelCount)
        .beEqualTo(testCount) && success;

    if (nodeOptions.expectedChannelCount && nodeOptions.expectedChannelCount.isFixed) {
        // The channel count is fixed.  Verify that we throw an error if we try to
        // change it. Arbitrarily set the count to be one more than the expected
        // value.
        testCount = nodeOptions.expectedChannelCount.value + 1;
        success = Should("new " + nodeName + "(c, {channelCount: " + testCount + "}}",
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCount: testCount
                        }));
            }).throw(nodeOptions.expectedChannelCount.errorType || "TypeError") && success;
    } else {
        // The channel count is not fixed.  Try to set the count to invalid
        // values and make sure an error is thrown.
        var errorType = "NotSupportedError";

        success = Should("new " + nodeName + "(c, {channelCount: 0}}",
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCount: 0
                        }));
            }).throw(errorType) && success;

        success = Should("new " + nodeName + "(c, {channelCount: 99}}",
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCount: 99
                        }));
            }).throw(errorType) && success;
    }

    // Test channelCountMode
    var testMode = "max";
    if (nodeOptions && nodeOptions.expectedChannelCountMode) {
        testMode = nodeOptions.expectedChannelCountMode.value;
    }
    success = Should("new " + nodeName + '(c, {channelCountMode: "' + testMode + '"}',
        function () {
            node = new window[nodeName](
                context,
                Object.assign({},
                    nodeOptions.additionalOptions, {
                        channelCountMode: testMode
                    }));
        }).notThrow() && success;
    success = Should("node.channelCountMode", node.channelCountMode)
        .beEqualTo(testMode) && success;

    if (nodeOptions.expectedChannelCountMode && nodeOptions.expectedChannelCountMode.isFixed) {
        // Channel count mode is fixed.  Test setting to something else throws.
        var testModeMap = {
            "max": "clamped-max",
            "clamped-max": "explicit",
            "explicit": "max"
        };
        testMode = testModeMap[nodeOptions.expectedChannelCountMode.value];
        success = Should("new " + nodeName + '(c, {channelCountMode: "' + testMode + '"}',
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCountMode: testMode
                        }));
            }).throw(nodeOptions.expectedChannelCountMode.errorType) && success;
    } else {
        // Mode is not fixed. Verify that we can set the mode to all valid
        // values, and that we throw for invalid values.

        success = Should("new " + nodeName + '(c, {channelCountMode: "clamped-max"}',
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCountMode: "clamped-max"
                        }));
            }).notThrow() && success;
        success = Should("node.channelCountMode", node.channelCountMode)
            .beEqualTo("clamped-max") && success;

        success = Should("new " + nodeName + '(c, {channelCountMode: "explicit"}',
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCountMode: "explicit"
                        }));
            }).notThrow() && success;
        success = Should("node.channelCountMode", node.channelCountMode)
            .beEqualTo("explicit") && success;

        success = Should("new " + nodeName + '(c, {channelCountMode: "foobar"}',
            function () {
                node = new window[nodeName](
                    context,
                    Object.assign({},
                        nodeOptions.additionalOptions, {
                            channelCountMode: "foobar"
                        }));
            }).throw("TypeError") && success;
        success = Should("node.channelCountMode", node.channelCountMode)
            .beEqualTo("explicit") && success;
    }

    // Test channelInterpretation
    success = Should("new " + nodeName + '(c, {channelInterpretation: "speakers"})',
        function () {
            node = new window[nodeName](
                context,
                Object.assign({},
                    nodeOptions.additionalOptions, {
                        channelInterpretation: "speakers"
                    }));
        }).notThrow() && success;
    success = Should("node.channelInterpretation", node.channelInterpretation)
        .beEqualTo("speakers") && success;

    success = Should("new " + nodeName + '(c, {channelInterpretation: "discrete"})',
        function () {
            node = new window[nodeName](
                context,
                Object.assign({},
                    nodeOptions.additionalOptions, {
                        channelInterpretation: "discrete"
                    }));
        }).notThrow() && success;
    success = Should("node.channelInterpretation", node.channelInterpretation)
        .beEqualTo("discrete") && success;

    success = Should("new " + nodeName + '(c, {channelInterpretation: "foobar"})',
        function () {
            node = new window[nodeName](
                context,
                Object.assign({},
                    nodeOptions.additionalOptions, {
                        channelInterpretation: "foobar"
                    }));
        }).throw("TypeError") && success;
    success = Should("node.channelInterpretation", node.channelInterpretation)
        .beEqualTo("discrete") && success;


    Should("AudioNodeOptions for " + nodeName, success)
        .summarize(
            "were correctly handled",
            "were not correctly handled");
}
