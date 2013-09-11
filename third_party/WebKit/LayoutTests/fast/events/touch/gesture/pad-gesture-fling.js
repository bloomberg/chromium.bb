description("Tests basic use of GestureFlingStart");

var actualWheelEventsOccurred = 0;
var cumulativeScrollX = 0;
var cumulativeScrollY = 0;

var minimumWheelEventsExpected = "40";
var minimumScrollXExpected = "200";
var minimumScrollYExpected = "200";

function recordWheelEvent(event)
{
    if (event.clientX != 10)
      debug('FAIL: clientX != 10');

    if (event.clientY != 11)
      debug('FAIL: event.clientY != 11');

    actualWheelEventsOccurred++;
    cumulativeScrollX += event.wheelDeltaX;
    cumulativeScrollY += event.wheelDeltaY;
}

document.addEventListener("mousewheel", recordWheelEvent);

if (window.testRunner && window.eventSender && window.eventSender.gestureFlingStart) {
    eventSender.gestureFlingStart(10, 11, 10000, 10000);
    testRunner.display();
    testRunner.display();
    testRunner.display();
}

setTimeout(function() {
    shouldBeGreaterThanOrEqual('actualWheelEventsOccurred', minimumWheelEventsExpected);
    shouldBeGreaterThanOrEqual('cumulativeScrollX', minimumScrollXExpected);
    shouldBeGreaterThanOrEqual('cumulativeScrollY', minimumScrollYExpected);
}, 100);

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
