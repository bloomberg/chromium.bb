'use strict';

// Creates a "user gesture" using Blink's test-only eventSender.
function triggerUserGesture() {
  if (!window.eventSender)
    throw new Error('The `eventSender` must be available for this test');

  eventSender.mouseDown();
  eventSender.mouseUp();
}
