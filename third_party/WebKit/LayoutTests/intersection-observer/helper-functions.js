// Some of the js-test.js boilerplate will add stuff to the top of the document early
// enough to screw with frame offsets that are measured by the test.  Delay all that
// jazz until the actual test code is finished.
setPrintTestResultsLazily();
var delayDescription = description;
var descriptionString = "";
var delayIsSuccessfullyParsed = isSuccessfullyParsed;
var isSuccessfullyParsed = function() {}
var description = function(msg) { descriptionString = msg }

if (window.testRunner)
  testRunner.waitUntilDone();

function rectToString(rect) {
  return "[" + rect.left + ", " + rect.right + ", " + rect.top + ", " + rect.bottom + "]";
}

function entryToString(entry) {
  var ratio = ((entry.intersectionRect.width * entry.intersectionRect.height) /
               (entry.boundingClientRect.width * entry.boundingClientRect.height));
  return (
      "boundingClientRect=" + rectToString(entry.boundingClientRect) + "\n" +
      "intersectionRect=" + rectToString(entry.intersectionRect) + "\n" +
      "visibleRatio=" + ratio + "\n" +
      "rootBounds=" + rectToString(entry.rootBounds) + "\n" +
      "target=" + entry.target + "\n" +
      "time=" + entry.time);
}

function finishTest() {
  if (descriptionString)
    delayDescription(descriptionString);
  delayIsSuccessfullyParsed();
  finishJSTest();
  if (window.testRunner)
    testRunner.notifyDone();
}
