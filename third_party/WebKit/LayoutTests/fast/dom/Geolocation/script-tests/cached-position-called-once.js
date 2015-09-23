description('Tests that when a cached position is available the callback for getCurrentPosition is called only once. This is a regression test for http://crbug.com/311876 .');

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPosition(document, 31.478, -0.166, 100);
internals.setGeolocationPermission(document, true);

// Only one success callback should be reported per call to getCurrentPosition.
var reportCount = 0;
function reportCallback(success, id) {
    isSuccess = success;
    shouldBeTrue('isSuccess');
    getCurrentPositionCallId = id;
    shouldBe('getCurrentPositionCallId', 'reportCount');
    if (++reportCount >= 3)
        finishJSTest();
}

var getCurrentPositionCall = 0;
function getPosition(milliseconds) {
    var id = getCurrentPositionCall++;
    var fn = function() {
        navigator.geolocation.getCurrentPosition(
                function(position) {
                    reportCallback(true, id);
                },
                function(error) {
                    reportCallback(false, id);
                },
                { maximumAge:600000, timeout:0 });
    };
    setTimeout(fn, milliseconds);
}

// The test terminates at the 3rd reported callback. If the bug still exists
// this happens after the 2nd call to getCurrentPosition, one of them is a
// repeat of the first.
getPosition(0);
getPosition(100);
getPosition(200);

window.jsTestIsAsync = true;
