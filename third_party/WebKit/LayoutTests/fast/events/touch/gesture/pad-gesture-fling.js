description("Tests basic use of GestureFlingStart");

var actualWheelEventsOccurred = 0;
var cumulativeScrollX = 0;
var cumulativeScrollY = 0;

var minimumWheelEventsExpected = 40;
var minimumScrollXExpected = "200";
var minimumScrollYExpected = "200";

var positionX = 10;
var positionY = 11;
var velocityX = 10000;
var velocityY = 10000;

function recordWheelEvent(event)
{
    if (event.clientX != 10)
      debug('FAIL: clientX != 10');

    if (event.clientY != 11)
      debug('FAIL: event.clientY != 11');

    actualWheelEventsOccurred++;
    cumulativeScrollX += event.wheelDeltaX;
    cumulativeScrollY += event.wheelDeltaY;

    if (actualWheelEventsOccurred == minimumWheelEventsExpected) {
      shouldBeGreaterThanOrEqual('cumulativeScrollX', minimumScrollXExpected);
      shouldBeGreaterThanOrEqual('cumulativeScrollY', minimumScrollYExpected);

      isSuccessfullyParsed();
      if (window.testRunner)
          testRunner.notifyDone();
    }
}

document.addEventListener("mousewheel", recordWheelEvent);

if (window.testRunner && window.eventSender && window.eventSender.gestureFlingStart) {
    eventSender.gestureFlingStart(positionX, positionY, velocityX, velocityY);
}

if (window.testRunner)
    testRunner.waitUntilDone();

